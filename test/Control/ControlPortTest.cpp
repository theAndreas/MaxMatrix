// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the control program's ports: the serial port against a pseudo
 * terminal, which stands in for a board_'s port as the kernel gives it to a
 * program, and the TCP port against the simulator's serial host on loopback. */
#include "ControlPort.h"

#include <gtest/gtest.h>
#include <poll.h>
#include <pty.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <string>

#include "ControlSerialPort.h"
#include "ControlTcpPort.h"
#include "Host/SimulatorSerialHost.h"
#include "MaxMatrixAttributes.h"

namespace {

uint16_t const kPort{18086};

// What arrives on `descriptor` within half a second.
std::string Receive(int descriptor) {
  pollfd ready{descriptor, POLLIN, 0};
  std::array<char, 256> chunk{};

  if (poll(&ready, 1, 500) <= 0) {
    return "";
  }
  ssize_t const length{read(descriptor, chunk.data(), chunk.size())};
  return (length > 0) ? std::string(chunk.data(), static_cast<size_t>(length))
                      : "";
}

// Reads `port` until something arrives or half a second has passed, as the
// window's timer would.
std::string ReadUntilReceived(ControlPort& port) {
  std::string received;

  int16_t i{0};
  while ((i < 50) && received.empty() && port.Read(received)) {
    usleep(10000);
    i++;
  }
  return received;
}

// Calls Task() of `host` until something arrives or half a second has passed,
// as the simulator's tick would.
std::string TaskUntilReceived(SimulatorSerialHost& host) {
  std::string received;

  int16_t i{0};
  while ((i < 50) && received.empty()) {
    received += host.Task();
    usleep(10000);
    i++;
  }
  return received;
}

// A pseudo terminal: `board_` is the board_'s end, the one the test writes and
// reads, and `name_` the port the program opens.
class ControlSerialPortTest : public testing::Test {
 protected:
  int board_{-1};
  int port_end_{-1};
  std::string name_;

  void SetUp() override {
    std::array<char, 64> path{};
    ASSERT_EQ(openpty(&board_, &port_end_, path.data(), nullptr, nullptr), 0);
    name_ = path.data();
  }
  void TearDown() override {
    if (port_end_ >= 0) {
      close(port_end_);
    }
    if (board_ >= 0) {
      close(board_);
    }
  }
};

TEST_F(ControlSerialPortTest, IsOpenOnceOpened) {
  ControlSerialPort port;

  ASSERT_TRUE(port.Open(name_, 115200));

  EXPECT_TRUE(port.IsOpen());
}

// A line goes out as it is, and what the board_ answers comes in, raw: no echo,
// no \n turned into \r\n.
TEST_F(ControlSerialPortTest, WritesALineAsItIs) {
  ControlSerialPort port;
  ASSERT_TRUE(port.Open(name_, 115200));

  ASSERT_TRUE(port.Write("text=Hi\n"));

  EXPECT_EQ(Receive(board_), "text=Hi\n");
}

TEST_F(ControlSerialPortTest, ReadsWhatTheBoardSaysRaw) {
  ControlSerialPort port;
  ASSERT_TRUE(port.Open(name_, 115200));
  ASSERT_EQ(write(board_, "ok\r\n", 4), 4);

  std::string const received{ReadUntilReceived(port)};

  EXPECT_EQ(received, "ok\r\n");
}

// a read without data returns at once rather than waiting, as the window reads
// from a timer
TEST_F(ControlSerialPortTest, ReadDoesNotWait) {
  ControlSerialPort port;
  std::string received;
  ASSERT_TRUE(port.Open(name_, 115200));

  bool const open{port.Read(received)};

  EXPECT_TRUE(open);
  EXPECT_EQ(received, "");
}

// A board_ unplugged: the read says the port is gone, and the port closes
// itself, rather than reporting no data for ever, which a read that returned 0
// would have done with VMIN 0.
TEST_F(ControlSerialPortTest, ReadSaysWhenTheBoardIsGone) {
  ControlSerialPort port;
  std::string received;
  ASSERT_TRUE(port.Open(name_, 115200));
  close(board_);
  board_ = -1;

  bool const open{port.Read(received)};

  EXPECT_FALSE(open);
}

TEST_F(ControlSerialPortTest, ClosesItselfWhenTheBoardIsGone) {
  ControlSerialPort port;
  std::string received;
  ASSERT_TRUE(port.Open(name_, 115200));
  close(board_);
  board_ = -1;

  max_matrix::Discard(port.Read(received));  // the port is asked afterwards

  EXPECT_FALSE(port.IsOpen());
}

// the Arduino IDE's serial monitor opened beside the program would take half of
// what the board_ says
TEST_F(ControlSerialPortTest, KeepsTheBoardToItself) {
  ControlSerialPort port;
  ControlSerialPort second;
  ASSERT_TRUE(port.Open(name_, 115200));

  bool const opened{second.Open(name_, 115200)};

  EXPECT_FALSE(opened);
}

// A rate the port does not take, and a port that does not exist, fail rather
// than open something else.
TEST_F(ControlSerialPortTest, RefusesARateItDoesNotTake) {
  ControlSerialPort port;

  bool const opened{port.Open(name_, 12345)};

  EXPECT_FALSE(opened);
  EXPECT_FALSE(port.IsOpen());
}

TEST_F(ControlSerialPortTest, RefusesAPortThatDoesNotExist) {
  ControlSerialPort port;

  bool const opened{port.Open("/dev/no-such-port", 115200)};

  EXPECT_FALSE(opened);
  EXPECT_FALSE(port.IsOpen());
}

// The control program and the simulator's serial host, each from its own end.
TEST(ControlTcpPortTest, WritesALineToTheSimulator) {
  SimulatorSerialHost host;
  ControlTcpPort port;
  ASSERT_TRUE(host.Begin(kPort));
  ASSERT_TRUE(port.Open(kPort));

  ASSERT_TRUE(port.Write("clear\n"));

  EXPECT_EQ(TaskUntilReceived(host), "clear\n");
}

TEST(ControlTcpPortTest, ReadsWhatTheSimulatorSays) {
  SimulatorSerialHost host;
  ControlTcpPort port;
  ASSERT_TRUE(host.Begin(kPort));
  ASSERT_TRUE(port.Open(kPort));
  // the host takes the connection; nothing has been sent that it could return
  max_matrix::Discard(host.Task());
  host.Send("ok\r\n");

  std::string const received{ReadUntilReceived(port)};

  EXPECT_EQ(received, "ok\r\n");
}

// A simulator that stops, or one that has a control program already and closes
// the second: the port says it is gone.
TEST(ControlTcpPortTest, ClosesItselfWhenTheSimulatorIsGone) {
  ControlTcpPort port;
  {
    SimulatorSerialHost host;
    ASSERT_TRUE(host.Begin(kPort));
    ASSERT_TRUE(port.Open(kPort));
    // the host takes the connection; nothing has been sent that it could return
    max_matrix::Discard(host.Task());
  }

  max_matrix::Discard(ReadUntilReceived(port));  // the port is asked afterwards

  EXPECT_FALSE(port.IsOpen());
}

// No simulator on the port: Open() fails at once, so that the window can say
// how to start one.
TEST(ControlTcpPortTest, OpenFailsWithoutASimulator) {
  ControlTcpPort port;

  bool const opened{port.Open(kPort)};

  EXPECT_FALSE(opened);
  EXPECT_FALSE(port.IsOpen());
}

}  // namespace
