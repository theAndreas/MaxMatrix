// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the simulator's serial host, the sketch's Serial on a TCP port, over
 * a real socket on loopback.
 *
 * simulator/tcp checks it with SerialControl behind it; these check what a
 * client may do that the sketch never sees: connect a second time, go away and
 * come back. On a port of its own, so that it needs no lock with the others. */
#include "Host/SimulatorSerialHost.h"

#include <gtest/gtest.h>
#include <netinet/in.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <string>

#include "MaxMatrixAttributes.h"

namespace {

uint16_t const kPort{18087};

// A client connected to the host on loopback, or -1 if the connection failed.
int Connect() {
  int const client{socket(AF_INET, SOCK_STREAM, 0)};
  if (client < 0) {
    return -1;
  }
  sockaddr_in address{};

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
  pollfd ready{client, POLLIN, 0};
  std::array<char, 256> chunk{};

  if (poll(&ready, 1, 500) <= 0) {
    return "";
  }
  ssize_t const length{recv(client, chunk.data(), chunk.size(), 0)};
  return (length > 0) ? std::string(chunk.data(), static_cast<size_t>(length))
                      : "";
}

// Whether the host has closed the connection of `client` within half a second:
// a read then returns 0.
bool IsClosedByHost(int client) {
  pollfd ready{client, POLLIN, 0};
  char c{0};

  return (poll(&ready, 1, 500) > 0) && (recv(client, &c, 1, 0) == 0);
}

// Calls Task() until something arrives or a second has passed, as the tick
// would.
std::string TaskUntilReceived(SimulatorSerialHost& host) {
  std::string received;

  int16_t i{0};
  while ((i < 100) && received.empty()) {
    received += host.Task();
    usleep(10000);
    i++;
  }
  return received;
}

// Calls Task() once, as the tick would, for the host to take or let go of a
// connection; nothing has been sent yet that it could return.
void Take(SimulatorSerialHost& host) { max_matrix::Discard(host.Task()); }

// what a client sends reaches the sketch's Serial as it is
TEST(SimulatorSerialHostTest, CarriesWhatAClientSendsToTheSketch) {
  SimulatorSerialHost host;
  ASSERT_TRUE(host.Begin(kPort));
  int const client{Connect()};
  ASSERT_GE(client, 0);
  ASSERT_EQ(send(client, "text=Hi\n", 8, 0), 8);

  std::string const received{TaskUntilReceived(host)};

  EXPECT_EQ(received, "text=Hi\n");
  close(client);
}

// what the sketch prints reaches the client
TEST(SimulatorSerialHostTest, CarriesWhatTheSketchPrintsToTheClient) {
  SimulatorSerialHost host;
  ASSERT_TRUE(host.Begin(kPort));
  int const client{Connect()};
  ASSERT_GE(client, 0);
  Take(host);

  host.Send("ok\r\n");

  EXPECT_EQ(Receive(client), "ok\r\n");
  close(client);
}

// a serial port has one program at its other end; a second client is told at
// once that the port is taken, rather than being left waiting for answers that
// go to the first
TEST(SimulatorSerialHostTest, ClosesASecondClient) {
  SimulatorSerialHost host;
  ASSERT_TRUE(host.Begin(kPort));
  int const first{Connect()};
  ASSERT_GE(first, 0);
  Take(host);
  int const second{Connect()};
  ASSERT_GE(second, 0);

  Take(host);

  EXPECT_TRUE(IsClosedByHost(second));
  close(first);
  close(second);
}

// the first client keeps its connection when a second is closed
TEST(SimulatorSerialHostTest, KeepsTheFirstClientBesideASecond) {
  SimulatorSerialHost host;
  ASSERT_TRUE(host.Begin(kPort));
  int const first{Connect()};
  ASSERT_GE(first, 0);
  Take(host);
  int const second{Connect()};
  ASSERT_GE(second, 0);
  Take(host);

  host.Send("ok\r\n");

  EXPECT_EQ(Receive(first), "ok\r\n");
  close(first);
  close(second);
}

// a control program that is closed and opened again finds the port free: the
// host lets go of a client that has gone
TEST(SimulatorSerialHostTest, TakesTheNextClientOnceTheFirstHasGone) {
  SimulatorSerialHost host;
  ASSERT_TRUE(host.Begin(kPort));
  int const first{Connect()};
  ASSERT_GE(first, 0);
  Take(host);
  close(first);
  Take(host);

  int const next{Connect()};
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
