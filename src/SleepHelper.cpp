#include "SleepHelper.h"

static Logger _log("app.sleep");

SleepHelper *SleepHelper::_instance;

// [static]
SleepHelper &SleepHelper::instance() {
    if (!_instance) {
        _instance = new SleepHelper();
    }
    return *_instance;
}

SleepHelper::SleepHelper() {
}

SleepHelper::~SleepHelper() {
}

void SleepHelper::setup() {
    // Call all setup functions
    for(auto it = setupFunctions.begin(); it != setupFunctions.end(); ++it) {
        (*it)(*this);
    }
}

void SleepHelper::loop() {
    // Call all loop functions
    for(auto it = loopFunctions.begin(); it != loopFunctions.end(); ++it) {
        (*it)(*this);
    }

    // Call the connection state handler
    stateHandler(*this);

}

bool SleepHelper::isSleepReady() {
    bool sleepReady = true;

    for(auto it = sleepReadyFunctions.begin(); it != sleepReadyFunctions.end(); ++it) {
        sleepReady = (*it)(*this);
        if (!sleepReady) {
            break;
        }
    }

    return sleepReady;
}

bool SleepHelper::shouldConnect() {

    for(auto it = shouldConnectFunctions.begin(); it != shouldConnectFunctions.end(); ++it) {
        ShouldConnectResult res = (*it)(*this);
        switch(res) {
            case ShouldConnectResult::FORCE_CONNECT:
                return true;

            case ShouldConnectResult::IF_TIME:
                break;

            case ShouldConnectResult::FORCE_NO_CONNECT:
                return false;
        }
    }
    
    // TODO: Check time to connect here


    return true;
}

bool SleepHelper::reachedMaximumTimeToConnect(long timeMs) {
    // Default value is false so if there are no maximum time to connect handlers, it will try forever
    bool res = false;

    for(auto it = maximumTimeToConnectFunctions.begin(); it != maximumTimeToConnectFunctions.end(); ++it) {
        bool res = (*it)(*this, timeMs);
        if (!res) {
            // This handler has not reached the minimum connected time yet, return false
            break;
        }
        // res is currently true. If this is the last handler, return true
    }

    return res;
}


bool SleepHelper::reachedMinimumConnectedTime(long timeMs) {  
    // Default value is false so if there are no minimum connected time handlers, you stay connected forever
    bool res = false;

    for(auto it = minimumConnectedTimeFunctions.begin(); it != minimumConnectedTimeFunctions.end(); ++it) {
        bool res = (*it)(*this, timeMs);
        if (!res) {
            // This handler has not reached the minimum connected time yet, return false
            break;
        }
        // res is currently true. If this is the last handler, return true
    }

    return res;
}


void SleepHelper::stateHandlerStart() {
    if (!shouldConnect()) {
        // We should not connect, so wait in this state
        return;
    }

    Particle.connect();    
    stateHandler = &SleepHelper::stateHandlerConnectWait;
    connectAttemptStartMillis = millis();
}


void SleepHelper::stateHandlerConnectWait() {
    if (Particle.connected()) {
        connectedStartMillis = millis();
        stateHandler = &SleepHelper::stateHandlerConnected;
        return;
    }
    unsigned long elapsedMs = millis() - connectAttemptStartMillis;
    if (reachedMaximumTimeToConnect(elapsedMs)) {
        stateHandler = &SleepHelper::stateHandlerPrepareToSleep;
        return;
    }

}

void SleepHelper::stateHandlerConnected() {
    if (!Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerReconnectWait;
        return;        
    }
    unsigned long elapsedMs = millis() - connectedStartMillis;
    if (reachedMinimumConnectedTime(elapsedMs)) {
        // Reached the minimum connected time, we may want to disconnect and sleep
        stateHandler = SleepHelper::stateHandlerPrepareToSleep;
        return;
    }
}

void SleepHelper::stateHandlerReconnectWait() {
    if (Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerConnected;
        return;
    }
}

void SleepHelper::stateHandlerPrepareToSleep() {

    // Disconnect from the cloud

    // Do we need to power down the cellular modem?

    SystemSleepConfiguration sleepConfig;

    // Calculate sleep duration

    // Allow other sleep configuration to be overridden
    for(auto it = sleepConfigurationFunctions.begin(); it != sleepConfigurationFunctions.end(); ++it) {
        (*it)(*this);
    }


    // Sleep!
    

    // Woke from sleep
    stateHandler = &SleepHelper::stateHandlerStart;

}



