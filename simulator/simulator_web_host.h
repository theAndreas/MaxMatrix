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
/**     \file       simulator_web_host.h
 *      \brief      A small HTTP and WebSocket server for the simulator, on loopback only
 *
 *      \details    Serves the page and keeps the WebSockets open to it. Ported from Wordclock's
 *                  platform/simulator/src/WebHost.cpp, without what only the clock needs (the firmware update route, the
 *                  JSON documents, Windows).
 *
 *****************************************************************************************************************************************************/
#ifndef MAXMATRIX_SIMULATOR_SIMULATOR_WEB_HOST_H_
#define MAXMATRIX_SIMULATOR_SIMULATOR_WEB_HOST_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include <vector>


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
    int listener_;
    int clients_[SIMULATOR_WEB_HOST_MAX_CLIENTS];
    std::string pending_[SIMULATOR_WEB_HOST_MAX_CLIENTS];
    char const* page_path_;

    void AcceptClients();
    void ReadClient(uint8_t, std::vector<std::string>*);
    bool AnswerRequest(int, std::string const&);
    void SendPage(int);
    uint8_t FindFreeSlot() const;
    void DropClient(uint8_t);
    static int OpenListener(uint16_t);
    static std::string ReadRequest(int);
    static bool GetPath(std::string const&, std::string*);
    static void SetNonBlocking(int);
    static void SetReceiveTimeout(int, uint32_t);
    static bool SendAll(int, std::string const&);
    static void SendResponse(int, char const*, char const*, std::string const&);

  public:
    SimulatorWebHost();

    // methods
    bool Begin(uint16_t, char const*);
    void Task(std::vector<std::string>*);
    void Broadcast(std::string const&);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
