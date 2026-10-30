/* Tests of the simulator's web host, over a real socket on loopback.
 *
 * simulator/web checks what a browser gets that sends its requests. What it cannot see is a client that connects and
 * says nothing, which the host has to survive without stopping the sketch. On its own port, so that it needs no lock
 * with simulator/web. */
#include <gtest/gtest.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>
#include "SimulatorWebHost.h"

static const uint16_t Port = 18081;

/* a connection that is opened and sends nothing is what a browser does when it opens a socket ahead of a request it
 * may never make; task() used to wait for it in recv() until the other end closed it, and the sketch waited with it.
 * The socket is shut down from a second thread after three seconds, so that a host that waits ends the test late and
 * failed rather than never. */
TEST(SimulatorWebHostTest, TaskDoesNotWaitForAConnectionThatSaysNothing)
{
    SimulatorWebHost Host;
    std::vector<std::string> Messages;
    sockaddr_in Address = sockaddr_in();

    ASSERT_EQ(Host.begin(Port, "/dev/null"), E_OK);
    const int Idle = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(Idle, 0);
    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(connect(Idle, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)), 0);

    std::promise<void> TaskDone;
    std::future<void> Done = TaskDone.get_future();
    std::thread Closer([Idle, &Done]() {
        if(Done.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) shutdown(Idle, SHUT_RDWR);
    });

    const auto Start = std::chrono::steady_clock::now();
    Host.task(&Messages);
    const auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Start);

    TaskDone.set_value();
    Closer.join();
    close(Idle);
    EXPECT_LT(Elapsed.count(), 1000) << "milliseconds in task()";
}
