// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the control program's ports: the serial port against a pseudo
 * terminal, which stands in for a board_'s port as the kernel gives it to a
 * program, and the TCP port against the simulator's serial host on loopback. */
#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <pty.h>
#include <unistd.h>

#include <array>
#include <string>

#include "ControlSerialPort.h"
#include "ControlTcpPort.h"
#include "Host/SimulatorSerialHost.h"

namespace {

uint16_t const kPort = 18086;

// What arrives on `descriptor` within half a second.
std::string Receive(int descriptor) {
  pollfd ready = {descriptor, POLLIN, 0};
  std::array<char, 256> chunk{};

  if (poll(&ready, 1, 500) <= 0) return "";
  ssize_t const length = read(descriptor, chunk.data(), chunk.size());
  return length > 0 ? std::string(chunk.data(), static_cast<size_t>(length))
                    : "";
}

// Reads `port` until something arrives or half a second has passed, as the
// window's timer would.
std::string ReadUntilReceived(ControlPort& port) {
  std::string received;

  for (int16_t i = 0; i < 50 && received.empty() && port.Read(&received); i++) {
    usleep(10000);
  }
  return received;
}

// A pseudo terminal: `board_` is the board_'s end, the one the test writes and
// reads, and `name_` the port the program opens.
class ControlSerialPortTest : public testing::Test {
 protected:
  int board_ = -1;
  int port_end_ = -1;
  std::string name_;

  void SetUp() override {
    std::array<char, 64> path{};
    ASSERT_EQ(openpty(&board_, &port_end_, path.data(), nullptr, nullptr), 0);
    name_ = path.data();
  }
  void TearDown() override {
    if (port_end_ >= 0) close(port_end_);
    if (board_ >= 0) close(board_);
  }
};

// a line goes out as it is, and what the board_ answers comes in, raw: no echo,
// no \n turned into \r\n
TEST_F(ControlSerialPortTest, CarriesBothWays) {
  ControlSerialPort port;

  ASSERT_TRUE(port.Open(name_, 115200));
  EXPECT_TRUE(port.IsOpen());
  EXPECT_TRUE(port.Write("text=Hi\n"));
  EXPECT_EQ(Receive(board_), "text=Hi\n");
  ASSERT_EQ(write(board_, "ok\r\n", 4), 4);
  EXPECT_EQ(ReadUntilReceived(port), "ok\r\n");
}

// a read without data returns at once rather than waiting, as the window reads
// from a timer
TEST_F(ControlSerialPortTest, ReadDoesNotWait) {
  ControlSerialPort port;
  std::string received;

  ASSERT_TRUE(port.Open(name_, 115200));
  EXPECT_TRUE(port.Read(&received));
  EXPECT_EQ(received, "");
}

// a board_ unplugged: the read says the port is gone, and the port closes
// itself, rather than reporting no data for ever, which a read that returned 0
// would have done with VMIN 0
TEST_F(ControlSerialPortTest, ReadSaysWhenTheBoardIsGone) {
  ControlSerialPort port;
  std::string received;

  ASSERT_TRUE(port.Open(name_, 115200));
  close(board_);
  board_ = -1;
  EXPECT_FALSE(port.Read(&received));
  EXPECT_FALSE(port.IsOpen());
}

// the Arduino IDE's serial monitor opened beside the program would take half of
// what the board_ says
TEST_F(ControlSerialPortTest, KeepsTheBoardToItself) {
  ControlSerialPort port;
  ControlSerialPort second;

  ASSERT_TRUE(port.Open(name_, 115200));
  EXPECT_FALSE(second.Open(name_, 115200));
}

// a rate the port does not take, and a port that does not exist, fail rather
// than open something else
TEST_F(ControlSerialPortTest, RefusesWhatItCannotOpen) {
  ControlSerialPort port;

  EXPECT_FALSE(port.Open(name_, 12345));
  EXPECT_FALSE(port.Open("/dev/no-such-port", 115200));
  EXPECT_FALSE(port.IsOpen());
}

// the control program and the simulator's serial host, each from its own end
TEST(ControlTcpPortTest, CarriesBothWaysToTheSimulator) {
  SimulatorSerialHost host;
  ControlTcpPort port;
  std::string received;

  ASSERT_TRUE(host.Begin(kPort));
  ASSERT_TRUE(port.Open(kPort));
  EXPECT_TRUE(port.Write("clear\n"));
  for (int16_t i = 0; i < 50 && received.empty(); i++) {
    host.Task(&received);
    usleep(10000);
  }
  EXPECT_EQ(received, "clear\n");
  host.Send("ok\r\n");
  EXPECT_EQ(ReadUntilReceived(port), "ok\r\n");
}

// a simulator that stops, or one that has a control program already and closes
// the second: the port says it is gone
TEST(ControlTcpPortTest, ReadSaysWhenTheSimulatorIsGone) {
  ControlTcpPort port;
  std::string received;

  {
    SimulatorSerialHost host;
    ASSERT_TRUE(host.Begin(kPort));
    ASSERT_TRUE(port.Open(kPort));
    host.Task(&received);
  }
  for (int16_t i = 0; i < 50 && port.Read(&received); i++) usleep(10000);
  EXPECT_FALSE(port.IsOpen());
}

// no simulator on the port: Open() fails at once, so that the window can say
// how to start one
TEST(ControlTcpPortTest, OpenFailsWithoutASimulator) {
  ControlTcpPort port;

  EXPECT_FALSE(port.Open(kPort));
  EXPECT_FALSE(port.IsOpen());
}

}  // namespace
