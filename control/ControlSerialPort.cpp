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
#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "ControlSerialPort.h"

namespace {

/* ControlSerialPort Parameter */
constexpr size_t kChunkLength{256U};
constexpr int kWriteTimeoutMs{1000};

// Whether the port takes `baud_rate`: the rates of the Arduino IDE's serial
// monitor that termios names on every system, up to the 115200 of
// SerialControl.
bool IsBaudRate(uint32_t baud_rate) {
  return baud_rate == 9600 || baud_rate == 19200 || baud_rate == 38400 ||
         baud_rate == 57600 || baud_rate == 115200;
}

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

bool ControlSerialPort::Read(std::string* data) {
  std::array<char, kChunkLength> chunk;
  DWORD const size = static_cast<DWORD>(chunk.size());
  DWORD length = size;

  if (!IsOpen()) return false;
  while (length == size) {
    if (ReadFile(handle_, chunk.data(), size, &length, nullptr) == 0) {
      Close();
      return false;
    }
    data->append(chunk.data(), length);
  }
  return true;
}

void ControlSerialPort::Close() {
  if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
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
  termios settings = termios();

  Close();
  if (!IsBaudRate(baud_rate)) {
    return false;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's open()
  descriptor_ = open(name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (descriptor_ < 0) {
    return false;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's ioctl()
  bool opened = ioctl(descriptor_, TIOCEXCL) == 0 &&
                tcgetattr(descriptor_, &settings) == 0;
  cfmakeraw(&settings);
  settings.c_cflag |= CLOCAL | CREAD;
  settings.c_cflag &= ~(CSTOPB | CRTSCTS);
  settings.c_cc[VMIN] = 1;
  settings.c_cc[VTIME] = 0;
  // glibc's cfsetspeed() takes a rate as its number as well as its B constant,
  // so no table maps one to the other.
  opened = opened && cfsetspeed(&settings, baud_rate) == 0 &&
           tcsetattr(descriptor_, TCSANOW, &settings) == 0;
  if (opened) {
    tcflush(descriptor_, TCIOFLUSH);
  } else {
    Close();
  }
  return opened;
}

std::vector<std::string> ControlSerialPort::List() {
  std::vector<std::string> ports;
  std::array<char const*, 2> const patterns{{"/dev/ttyACM*", "/dev/ttyUSB*"}};

  for (char const* const pattern : patterns) {
    glob_t found = glob_t();
    if (glob(pattern, 0, nullptr, &found) == 0) {
      for (size_t i = 0; i < found.gl_pathc; i++) {
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
  size_t sent = 0;

  if (!IsOpen()) {
    return false;
  }
  while (sent < data.size()) {
    ssize_t const written = write(descriptor_, &data[sent], data.size() - sent);
    pollfd writable = {descriptor_, POLLOUT, 0};
    if (written > 0) {
      sent += static_cast<size_t>(written);
    } else if (written < 0 && errno == EAGAIN &&
               poll(&writable, 1, kWriteTimeoutMs) > 0) {
      continue;
    } else {
      Close();
      return false;
    }
  }
  return true;
}

bool ControlSerialPort::Read(std::string* data) {
  std::array<char, kChunkLength> chunk{};

  if (!IsOpen()) {
    return false;
  }
  for (;;) {
    ssize_t const length = read(descriptor_, chunk.data(), chunk.size());
    if (length > 0) {
      data->append(chunk.data(), static_cast<size_t>(length));
    } else if (length < 0 && errno == EAGAIN) {
      return true;
    } else {
      Close();
      return false;
    }
  }
}

void ControlSerialPort::Close() {
  if (descriptor_ >= 0) {
    close(descriptor_);
  }
  descriptor_ = -1;
}

#endif
