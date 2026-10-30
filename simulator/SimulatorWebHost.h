/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       SimulatorWebHost.h
 *      \brief      A small HTTP and WebSocket server for the simulator, on loopback only
 *
 *      \details    Serves the page and keeps the WebSockets open to it. Ported from Wordclock's
 *                  platform/simulator/src/WebHost.cpp, without what only the clock needs (the firmware update route, the
 *                  JSON documents, Windows).
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_WEB_HOST_H_
#define _SIMULATOR_WEB_HOST_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include <vector>
#include "MaxMatrixStandardTypes.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorWebHost Parameter */
#define SIMULATOR_WEB_HOST_MAX_CLIENTS                              4u


/******************************************************************************************************************************************************
 *  CLASS  SimulatorWebHost
 *****************************************************************************************************************************************************/
/* Polled from the simulator's tick rather than run in a thread, so nothing here needs a lock: every call happens between
 * two steps of the sketch. */
class SimulatorWebHost
{
  private:
    int Listener;
    int Clients[SIMULATOR_WEB_HOST_MAX_CLIENTS];
    std::string Pending[SIMULATOR_WEB_HOST_MAX_CLIENTS];
    const char* PagePath;

    void acceptClients();
    void readClient(unsigned, std::vector<std::string>*);
    bool answerRequest(int, const std::string&);
    void sendPage(int);
    unsigned findFreeSlot() const;
    void dropClient(unsigned);
    static int openListener(unsigned short);
    static std::string readRequest(int);
    static stdReturnType getPath(const std::string&, std::string*);
    static void setNonBlocking(int);
    static void setReceiveTimeout(int, unsigned);
    static bool sendAll(int, const std::string&);
    static void sendResponse(int, const char*, const char*, const std::string&);

  public:
    SimulatorWebHost();

    // methods
    stdReturnType begin(unsigned short, const char*);
    void task(std::vector<std::string>*);
    void broadcast(const std::string&);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
