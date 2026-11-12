// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the simulator's serial host, the sketch's Serial on a TCP port, over
 * a real socket on loopback.
 *
 * simulator/tcp checks it with SerialControl behind it; these check what a
 * client may do that the sketch never sees: connect a second time, go away and
 * come back. On a port of its own, so that it needs no lock with the others. */
#include "Host/SimulatorSerialHost.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <string>

namespace {

uint16_t const kPort = 18087;

// A client connected to the host on loopback, or -1 if the connection failed.
int Connect() {
  int const client = socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    return -1;
  }
  sockaddr_in address = sockaddr_in();

  address.sin_family = AF_INET;
  address.sin_port = htons(kPort);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) !=
      0) {
    close(client);
    return -1;
  }
  return client;
}

// What arrives at `client` within half a second; "" if the host closed the
// connection or sent nothing.
std::string Receive(int client) {
  pollfd ready = {client, POLLIN, 0};
  std::array<char, 256> chunk{};

  if (poll(&ready, 1, 500) <= 0) {
    return "";
  }
  ssize_t const length = recv(client, chunk.data(), chunk.size(), 0);
  return length > 0 ? std::string(chunk.data(), static_cast<size_t>(length))
                    : "";
}

// Whether the host has closed the connection of `client` within half a second:
// a read then returns 0.
bool IsClosedByHost(int client) {
  pollfd ready = {client, POLLIN, 0};
  char c = 0;

  return poll(&ready, 1, 500) > 0 && recv(client, &c, 1, 0) == 0;
}

// Calls Task() until something arrives or a second has passed, as the tick
// would.
std::string TaskUntilReceived(SimulatorSerialHost& host) {
  std::string received;

  for (int16_t i = 0; i < 100 && received.empty(); i++) {
    host.Task(&received);
    usleep(10000);
  }
  return received;
}

// what a client sends reaches the sketch's Serial as it is, and what the sketch
// prints reaches the client
TEST(SimulatorSerialHostTest, CarriesBothWays) {
  SimulatorSerialHost host;

  ASSERT_TRUE(host.Begin(kPort));
  int const client = Connect();
  ASSERT_GE(client, 0);

  ASSERT_EQ(send(client, "text=Hi\n", 8, 0), 8);
  EXPECT_EQ(TaskUntilReceived(host), "text=Hi\n");
  host.Send("ok\r\n");
  EXPECT_EQ(Receive(client), "ok\r\n");
  close(client);
}

// a serial port has one program at its other end; a second client is told at
// once that the port is taken, rather than being left waiting for answers that
// go to the first, which keeps its connection
TEST(SimulatorSerialHostTest, ClosesASecondClient) {
  SimulatorSerialHost host;
  std::string received;

  ASSERT_TRUE(host.Begin(kPort));
  int const first = Connect();
  ASSERT_GE(first, 0);
  host.Task(&received);
  int const second = Connect();
  ASSERT_GE(second, 0);
  host.Task(&received);

  EXPECT_TRUE(IsClosedByHost(second));
  host.Send("ok\r\n");
  EXPECT_EQ(Receive(first), "ok\r\n");
  close(first);
  close(second);
}

// a control program that is closed and opened again finds the port free: the
// host lets go of a client that has gone
TEST(SimulatorSerialHostTest, TakesTheNextClientOnceTheFirstHasGone) {
  SimulatorSerialHost host;
  std::string received;

  ASSERT_TRUE(host.Begin(kPort));
  int const first = Connect();
  ASSERT_GE(first, 0);
  host.Task(&received);
  close(first);
  host.Task(&received);

  int const next = Connect();
  ASSERT_GE(next, 0);
  ASSERT_EQ(send(next, "clear\n", 6, 0), 6);
  EXPECT_EQ(TaskUntilReceived(host), "clear\n");
  close(next);
}

// the port is free again once the host is gone, so that a simulator in the same
// process, or the next test, can open it
TEST(SimulatorSerialHostTest, LetsGoOfThePortWhenDestroyed) {
  {
    SimulatorSerialHost host;
    ASSERT_TRUE(host.Begin(kPort));
  }
  SimulatorSerialHost host;
  EXPECT_TRUE(host.Begin(kPort));
}

}  // namespace
