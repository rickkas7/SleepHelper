#include "SleepHelper.h"

#include <fcntl.h>

SleepHelper *SleepHelper::_instance;

// [static]
SleepHelper &SleepHelper::instance() {
    if (!_instance) {
        _instance = new SleepHelper();
    }
    return *_instance;
}

SleepHelper::SleepHelper() : appLog("app.sleep") {
}

SleepHelper::~SleepHelper() {
}

#ifndef UNITTEST
void SleepHelper::setup() {
    // Register for system events
    System.on(firmware_update | firmware_update_pending | reset | out_of_memory, systemEventHandlerStatic);

    // Call all setup functions
    setupFunctions.forEach();

    // Called from setup(), and also after waking from sleep
    wakeOrBootFunctions.forEach();
}

void SleepHelper::loop() {
    // TODO: Check outOfMemory and reset here

    // Call all loop functions
    loopFunctions.forEach();

    // Call the connection state handler
    stateHandler(*this);

}

void SleepHelper::systemEventHandler(system_event_t event, int param) {
    switch(event) {
        case firmware_update:
            switch(param) {
                case firmware_update_begin:
                    break;

                case firmware_update_progress:
                    break;

                case firmware_update_complete:
                    break;

                case firmware_update_failed: 
                    break;
            }
            break;

        case firmware_update_pending:
            break;

        case reset:
            sleepOrResetFunctions.forEach(true);
            break;

        case out_of_memory:
            outOfMemory = true;
            break;
    }
}

// [static]
void SleepHelper::systemEventHandlerStatic(system_event_t event, int param) {
    SleepHelper::instance().systemEventHandler(event, param);
}


void SleepHelper::stateHandlerStart() {
    if (!shouldConnectFunctions.shouldConnect()) {
        // We should not connect, so go into no connection state
        appLog.info("running in no connection mode");
        stateHandler = &SleepHelper::stateHandlerNoConnection;
        return;
    }
    appLog.info("connecting to cloud");

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
    system_tick_t elapsedMs = millis() - connectAttemptStartMillis;

    if (maximumTimeToConnectFunctions.untilFalse(false, elapsedMs)) {
        appLog.info("timed out connecting to cloud");
        stateHandler = &SleepHelper::stateHandlerDisconnectBeforeSleep;
        return;
    }

}

void SleepHelper::stateHandlerConnected() {
    if (!Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerReconnectWait;
        return;        
    }

    appLog.info("connected to cloud");

    whileConnectedFunctions.forEach();

    system_tick_t elapsedMs = millis() - connectedStartMillis;
    if (sleepReadyFunctions.untilFalse(true, elapsedMs)) {
        // Ready to sleep, go into prepare to sleep state
        stateHandler = &SleepHelper::stateHandlerDisconnectBeforeSleep;
        return;
    }
    

}

void SleepHelper::stateHandlerReconnectWait() {
    if (Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerConnected;
        return;
    }
}

void SleepHelper::stateHandlerNoConnection() {

    if (!noConnectionFunctions.whileAnyTrue(false)) {
        // No more noConnectionFunctions need time, so go to sleep now
        stateHandler = &SleepHelper::stateHandlerPrepareToSleep;
        return;
    }
    
    // Stay in this state while any noConnectionFunction returns true
    return;
}

void SleepHelper::stateHandlerDisconnectBeforeSleep() {

    appLog.info("disconnecting from cloud");

    // Disconnect from the cloud
    Particle.disconnect();

    // Do we need to power down the cellular modem?
    stateHandler = &SleepHelper::stateHandlerPrepareToSleep;
}


void SleepHelper::stateHandlerPrepareToSleep() {
    appLog.info("stateHandlerPrepareToSleep");
    sleepOrResetFunctions.forEach(false);

    SystemSleepConfiguration sleepConfig;

    // Default sleep mode is ULP. Can override by sleepConfigurationFunction
    sleepConfig.mode(SystemSleepMode::ULTRA_LOW_POWER);

    // Calculate sleep duration (default to 15 minutes)
    std::chrono::milliseconds sleepTime = 15min;
    //sleepTimeRecommendationFunctions.forEach(sleepTime);

    // Allow other sleep configuration to be overridden
    sleepConfigurationFunctions.forEach(sleepConfig, sleepTime);
    sleepConfig.duration(sleepTime);

    appLog.info("sleeping for %d sec", (int)(sleepTime.count() / 1000));

    // Sleep!
    SystemSleepResult sleepResult = System.sleep(sleepConfig);

    // Woke from sleep
    stateHandler = &SleepHelper::stateHandlerStart;

    wakeOrBootFunctions.forEach();
}
#endif // UNITTEST

//
// SettingsFile
//
SleepHelper::SettingsFile *SleepHelper::SettingsFile::_settingsFile;


SleepHelper::SettingsFile &SleepHelper::SettingsFile::instance() {
    if (!_settingsFile) {
        _settingsFile = new SleepHelper::SettingsFile();
    }
    return *_settingsFile;
}

bool SleepHelper::SettingsFile::load() {
    WITH_LOCK(*this) {

        int fd = open(SETTINGS_PATH, O_RDONLY);
        if (fd != -1) {
            
            close(fd);
        }
        
    }
    return true;
}

bool SleepHelper::SettingsFile::save() {
    WITH_LOCK(*this) {

    }
    return true;
}



