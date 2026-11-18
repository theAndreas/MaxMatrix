// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A board's serial port, over `termios` on Linux and the Win32 API on
 *        Windows.
 *
 * Both halves keep to the same rules: the port opens raw, a read never waits,
 * and a port whose other end is gone closes itself, so that the window can say
 * so.
 */

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "ControlSerialPort.h"
#include "ControlStream.h"
#include "MaxMatrixAttributes.h"
#include "Utility/Bit.h"

namespace {

/* ControlSerialPort Parameter */
constexpr int kWriteTimeoutMs{1000};

/* the rates of the Arduino IDE's serial monitor that termios names on every
 * system, up to the 115200 of SerialControl */
constexpr std::array<uint32_t, 5> kBaudRates{
    {9600, 19200, 38400, 57600, 115200}};

// Whether the port takes `baud_rate`.
bool IsBaudRate(uint32_t baud_rate) {
  return std::find(kBaudRates.begin(), kBaudRates.end(), baud_rate) !=
         kBaudRates.end();
}

#ifndef _WIN32
// Whether the call that failed only would have had to wait.
bool WouldBlock() { return errno == EAGAIN; }
#endif

}  // namespace

#ifdef _WIN32

ControlSerialPort::ControlSerialPort() { handle_ = INVALID_HANDLE_VALUE; }

// The class is final, so Close() is its own here, as nothing can override it.
ControlSerialPort::~ControlSerialPort() { Close(); }

/*
 * A read returns at once with what has arrived, as ReadIntervalTimeout MAXDWORD
 * with no total timeout says; a write gives up after
 * kWriteTimeoutMs. DTR on, as the Arduino IDE has it,
 * which resets an Uno.
 */
bool ControlSerialPort::Open(std::string const& name, uint32_t baud_rate) {
  std::string const path = "\\\\.\\" + name;
  DCB settings = DCB();
  COMMTIMEOUTS timeouts = COMMTIMEOUTS();

  Close();
  if (!IsBaudRate(baud_rate)) return false;
  handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                        OPEN_EXISTING, 0, nullptr);
  if (handle_ == INVALID_HANDLE_VALUE) return false;

  settings.DCBlength = sizeof(settings);
  bool opened = GetCommState(handle_, &settings) != 0;
  settings.BaudRate = baud_rate;
  settings.ByteSize = 8;
  settings.Parity = NOPARITY;
  settings.StopBits = ONESTOPBIT;
  settings.fBinary = TRUE;
  settings.fParity = FALSE;
  settings.fOutxCtsFlow = FALSE;
  settings.fOutxDsrFlow = FALSE;
  settings.fOutX = FALSE;
  settings.fInX = FALSE;
  settings.fDtrControl = DTR_CONTROL_ENABLE;
  settings.fRtsControl = RTS_CONTROL_ENABLE;
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.WriteTotalTimeoutConstant = kWriteTimeoutMs;
  opened = opened && SetCommState(handle_, &settings) != 0 &&
           SetCommTimeouts(handle_, &timeouts) != 0;
  if (!opened) Close();
  return opened;
}

// Every name QueryDosDevice() knows that starts with COM, COM2 before COM10.
std::vector<std::string> ControlSerialPort::List() {
  std::vector<char> names(UINT16_MAX);
  std::vector<std::string> ports;

  DWORD const length =
      QueryDosDeviceA(nullptr, names.data(), static_cast<DWORD>(names.size()));
  for (DWORD start = 0; start < length && names[start] != '\0';) {
    std::string const name = names.data() + start;
    if (name.compare(0, 3, "COM") == 0) ports.push_back(name);
    start += static_cast<DWORD>(name.size() + 1);
  }
  std::sort(ports.begin(), ports.end(),
            [](std::string const& left, std::string const& right) {
              return left.size() != right.size() ? left.size() < right.size()
                                                 : left < right;
            });
  return ports;
}

bool ControlSerialPort::IsOpen() const {
  return handle_ != INVALID_HANDLE_VALUE;
}

bool ControlSerialPort::Write(std::string const& data) {
  DWORD written = 0;

  if (!IsOpen()) return false;
  if (WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()), &written,
                nullptr) == 0 ||
      written != data.size()) {
    Close();
    return false;
  }
  return true;
}

bool ControlSerialPort::Read(std::string& data) {
  std::array<char, kControlStreamChunkLength> chunk{};
  DWORD const size = static_cast<DWORD>(chunk.size());
  DWORD length = size;

  if (!IsOpen()) return false;
  while (length == size) {
    if (ReadFile(handle_, chunk.data(), size, &length, nullptr) == 0) {
      Close();
      return false;
    }
    data.append(chunk.data(), length);
  }
  return true;
}

void ControlSerialPort::Close() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    // The handle is gone even when it reports an error, and there is nothing
    // to try again.
    max_matrix::Discard(CloseHandle(handle_));
  }
  handle_ = INVALID_HANDLE_VALUE;
}

#else

ControlSerialPort::ControlSerialPort() = default;

// The class is final, so Close() is its own here, as nothing can override it.
ControlSerialPort::~ControlSerialPort() { Close(); }

/*
 * Raw, and VMIN 1 with O_NONBLOCK: a read without data then fails with EAGAIN,
 * while a read that returns 0 means the port has hung up, an Uno unplugged.
 * With VMIN 0 both would return 0. TIOCEXCL keeps a second program, the Arduino
 * IDE's serial monitor say, from opening the port while the control program has
 * it.
 */
bool ControlSerialPort::Open(std::string const& name, uint32_t baud_rate) {
  Close();
  if (!IsBaudRate(baud_rate)) {
    return false;
  }
  /* read and write, not as the terminal that controls this process, and
   * without waiting for the modem lines */
  constexpr int kOpenFlags{O_RDWR | O_NOCTTY | O_NONBLOCK};

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's open()
  descriptor_ = open(name.c_str(), kOpenFlags);
  if (descriptor_ < 0) {
    return false;
  }
  bool const opened{Configure(baud_rate)};
  if (opened) {
    // What lay in the buffers from before goes, if it can; left there, it
    // starts the first line read, which the link passes on as a line of its
    // own.
    max_matrix::Discard(tcflush(descriptor_, TCIOFLUSH));
  } else {
    Close();
  }
  return opened;
}

std::vector<std::string> ControlSerialPort::List() {
  std::vector<std::string> ports;
  std::array<char const*, 2> const patterns{{"/dev/ttyACM*", "/dev/ttyUSB*"}};

  for (char const* const pattern : patterns) {
    glob_t found{};
    if (glob(pattern, 0, nullptr, &found) == 0) {
      for (size_t i{0}; i < found.gl_pathc; i++) {
        // T-20: glob() hands its paths as a pointer and a count
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ports.emplace_back(found.gl_pathv[i]);
      }
    }
    globfree(&found);
  }
  std::sort(ports.begin(), ports.end());
  return ports;
}

bool ControlSerialPort::IsOpen() const { return descriptor_ >= 0; }

// A full output buffer is waited for, up to
// kWriteTimeoutMs: a line is a few bytes, and a port that
// takes none for that long has no board behind it.
bool ControlSerialPort::Write(std::string const& data) {
  if (!IsOpen()) {
    return false;
  }
  int const descriptor{descriptor_};
  bool const sent{ControlStream::SendAll(
      data,
      [descriptor](char const* bytes, size_t length) {
        return write(descriptor, bytes, length);
      },
      WouldBlock,
      [descriptor] {
        pollfd writable{descriptor, POLLOUT, 0};
        return poll(&writable, 1, kWriteTimeoutMs) > 0;
      })};
  if (!sent) {
    Close();
  }
  return sent;
}

bool ControlSerialPort::Read(std::string& data) {
  if (!IsOpen()) {
    return false;
  }
  int const descriptor{descriptor_};
  bool const open{ControlStream::ReceiveAvailable(
      data,
      [descriptor](char* bytes, size_t length) {
        return read(descriptor, bytes, length);
      },
      WouldBlock)};
  if (!open) {
    Close();
  }
  return open;
}

void ControlSerialPort::Close() {
  if (descriptor_ >= 0) {
    // close() frees the descriptor even when it reports an error, and there is
    // nothing to try again.
    max_matrix::Discard(close(descriptor_));
  }
  descriptor_ = -1;
}

// Takes the open port for itself and sets it up as Open() says. Returns false
// if it is taken or does not take the baud rate. FN-11: not const, as it
// changes the port this object stands for, through its descriptor.
// NOLINTNEXTLINE(readability-make-member-function-const)
bool ControlSerialPort::Configure(uint32_t baud_rate) {
  /* the modem lines ignored, and receiving on */
  constexpr tcflag_t kLocalReceive{CLOCAL | CREAD};
  /* two stop bits and the hardware handshake, both off for SerialControl */
  constexpr tcflag_t kTwoStopsHandshake{CSTOPB | CRTSCTS};
  termios settings{};

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's ioctl()
  bool const taken{ioctl(descriptor_, TIOCEXCL) == 0};
  bool const read{taken && (tcgetattr(descriptor_, &settings) == 0)};
  cfmakeraw(&settings);
  max_matrix::Bit<tcflag_t>::SetBits(settings.c_cflag, kLocalReceive);
  max_matrix::Bit<tcflag_t>::ClearBits(settings.c_cflag, kTwoStopsHandshake);
  settings.c_cc[VMIN] = 1;
  settings.c_cc[VTIME] = 0;
  // glibc's cfsetspeed() takes a rate as its number as well as its B constant,
  // so no table maps one to the other.
  bool const rate_set{read && (cfsetspeed(&settings, baud_rate) == 0)};
  return rate_set && (tcsetattr(descriptor_, TCSANOW, &settings) == 0);
}

#endif
