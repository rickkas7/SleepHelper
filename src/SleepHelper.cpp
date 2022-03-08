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
    setupFunctions.forEach();
}

void SleepHelper::loop() {
    // Call all loop functions
    loopFunctions.forEach();

    // Call the connection state handler
    // stateHandler(*this); // TEMPORARY COMMENT OUT

}




void SleepHelper::stateHandlerStart() {
    if (!shouldConnectFunctions.shouldConnect()) {
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
        stateHandler = &SleepHelper::stateHandlerPrepareToSleep;
        return;
    }
}

void SleepHelper::stateHandlerReconnectWait() {
    if (Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerConnected;
        return;
    }
}

/*
    
     * @brief Determine if it's a good time to go to sleep
     * 
     * The sleep ready functions registered with withSleepReadyFunction() are called. If any returns false (not ready)
     * then this function returns false. If all return true (or there are no sleep ready functions) then this function
     * returns true.
    
    bool isSleepReady() {
        return sleepReadyFunctions.untilFalse(true);
    }

*/

void SleepHelper::stateHandlerPrepareToSleep() {

    // Disconnect from the cloud

    // Do we need to power down the cellular modem?

    SystemSleepConfiguration sleepConfig;

    // Default sleep mode is ULP. Can override by sleepConfigurationFunction
    sleepConfig.mode(SystemSleepMode::ULTRA_LOW_POWER);

    // Calculate sleep duration

    // system_tick_t
    // sleepConfig.duration();

    // Allow other sleep configuration to be overridden
    sleepConfigurationFunctions.forEach(sleepConfig);

    // Sleep!
    System.sleep(sleepConfig);

    // Woke from sleep
    stateHandler = &SleepHelper::stateHandlerStart;

}



