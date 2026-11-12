// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the simulator's web host, over a real socket on loopback.
 *
 * simulator/web checks what a browser gets that sends its requests. What it
 * cannot see is a client that connects and says nothing, which the host has to
 * survive without stopping the sketch. On its own port, so that it needs no
 * lock with simulator/web. */
#include "Host/SimulatorWebHost.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

static uint16_t const kPort = 18081;

/* a connection that is opened and sends nothing is what a browser does when it
 * opens a socket ahead of a request it may never make; Task() used to wait for
 * it in recv() until the other end closed it, and the sketch waited with it.
 * The socket is shut down from a second thread after three seconds, so that a
 * host that waits ends the test late and failed rather than never. */
TEST(SimulatorWebHostTest, TaskDoesNotWaitForAConnectionThatSaysNothing) {
  SimulatorWebHost host;
  std::vector<std::string> messages;
  sockaddr_in address = sockaddr_in();

  ASSERT_TRUE(host.Begin(kPort, "/dev/null"));
  int const idle = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GE(idle, 0);
  address.sin_family = AF_INET;
  address.sin_port = htons(kPort);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  ASSERT_EQ(
      connect(idle, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);

  std::promise<void> task_done;
  std::future<void> done = task_done.get_future();
  std::thread closer([idle, &done] {
    if (done.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
      shutdown(idle, SHUT_RDWR);
    }
  });

  auto const start = std::chrono::steady_clock::now();
  host.Task(&messages);
  auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  task_done.set_value();
  closer.join();
  close(idle);
  EXPECT_LT(elapsed.count(), 1000) << "milliseconds in Task()";
}
