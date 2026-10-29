/* A small HTTP and WebSocket server for the simulator, on loopback only.
 *
 * Ported from Wordclock's platform/simulator/src/WebHost.cpp, without what only the clock needs (the firmware update
 * route, the JSON documents, Windows). Polled from the simulator's tick rather than run in a thread, so nothing here
 * needs a lock: every call happens between two steps of the sketch. */
#ifndef _SIMULATOR_WEB_HOST_H_
#define _SIMULATOR_WEB_HOST_H_

#include <string>

namespace WebHost {
    /* opens the port on localhost; false if it is taken, so that the caller can say so and run on without */
    bool begin(unsigned short Port, const char* PagePath);
    /* accepts and answers whatever has arrived; Command gets every text message a page sends */
    void task(void (*Command)(const std::string&));
    void broadcast(const std::string& Text);
    bool hasClients();
}

#endif
