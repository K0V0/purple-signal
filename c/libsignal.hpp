#pragma once

#include <memory>
#include <functional>
#include <purple.h>
#include "jni/purplesignal.hpp"

#define SIGNAL_PLUGIN_ID "prpl-hehoe-signal"

/*
 * Holds all information related to this connection instance.
 * 
 * Methods defer their work to the appropriate implementation (C → Java).
 */
class PurpleSignalConnection {
    public:
    PurpleAccount *account;
    PurpleConnection *connection;
    PurpleSignal ps;
    
    // setup
    PurpleSignalConnection(PurpleAccount *account, PurpleConnection *pc, const std::string & signal_lib_directory);
    
    // connecting and disconnecting
    void login(const char *username, const std::string & settings_directory);
    int close();
    
    // account management
    void link_account();
    void register_account(bool voice);
    void verify_account(const std::string & code, const std::string & pin);
    
    // messaging
    int send(const char *who, const char *message);
};
