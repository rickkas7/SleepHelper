#include "SleepHelper.h"

#include <fcntl.h>

static constexpr const char * const SETTINGS_PATH = 
#ifndef UNITTEST
    "/usr/sleepSettings.json";
#else
    "./sleepSettings.json";
#endif    

SleepHelper *SleepHelper::_instance;

// [static]
SleepHelper &SleepHelper::instance() {
    if (!_instance) {
        _instance = new SleepHelper();
    }
    return *_instance;
}

SleepHelper::SleepHelper() : appLog("app.sleep"), settingsFile(SETTINGS_PATH) {
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

    wakeFunctions.forEach(sleepResult);

    // Woke from sleep
    stateHandler = &SleepHelper::stateHandlerStart;

    wakeOrBootFunctions.forEach();
}
#endif // UNITTEST

//
// SettingsFile
//

bool SleepHelper::SettingsFile::load() {
    WITH_LOCK(*this) {
        bool loaded = false;

        int dataSize = 0;

        int fd = open(path, O_RDONLY);
        if (fd != -1) {
            dataSize = read(fd, parser.getBuffer(), parser.getBufferLen());
            if (dataSize > 0) {                
                parser.setOffset(dataSize);
                if (parser.parse()) {
                    loaded = true;
                }
            }
            close(fd);
        }
        
        if (!loaded) {
            parser.addString("{}");
            parser.parse();
        }
    }

    return true;
}

bool SleepHelper::SettingsFile::save() {
    WITH_LOCK(*this) {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
        if (fd != -1) {            
            write(fd, parser.getBuffer(), parser.getOffset());
            close(fd);
        }
        else {            
            return false;
        }
    }

    return true;
}

bool SleepHelper::SettingsFile::setValuesJson(const char *inputJson) {
    std::vector<String> updatedKeys;

    WITH_LOCK(*this) {
        JsonParserStatic<particle::protocol::MAX_EVENT_DATA_LENGTH, 50> inputParser;
        inputParser.addString(inputJson);
        inputParser.parse();

        const JsonParserGeneratorRK::jsmntok_t *keyToken;
		const JsonParserGeneratorRK::jsmntok_t *valueToken;

        for(size_t index = 0; ; index++) {
    		if (!inputParser.getKeyValueTokenByIndex(inputParser.getOuterObject(), keyToken, valueToken, index)) {
                break;
            }

            String key;
            inputParser.getTokenValue(keyToken, key);

            JsonModifier modifier(parser);

            // Does this item exist?
    		const JsonParserGeneratorRK::jsmntok_t *oldValueToken;
            if (!parser.getValueTokenByKey(parser.getOuterObject(), key, oldValueToken)) {
                // Key does not exist, insert a dummy key/value
                modifier.insertOrUpdateKeyValue(parser.getOuterObject(), key, (int)0);

                parser.getValueTokenByKey(parser.getOuterObject(), key, oldValueToken);
            }

            int valueLen = valueToken->end - valueToken->start;
            int oldValueLen = oldValueToken->end - oldValueToken->start;

            if (valueToken->type != oldValueToken->type || 
                valueLen != oldValueLen ||
                memcmp(inputParser.getBuffer() + valueToken->start, parser.getBuffer() + oldValueToken->start, valueLen) != 0) {

                const JsonParserGeneratorRK::jsmntok_t expandedValueToken = modifier.tokenWithQuotes(valueToken);
                const JsonParserGeneratorRK::jsmntok_t expandedOldValueToken = modifier.tokenWithQuotes(oldValueToken);
                modifier.startModify(&expandedOldValueToken);
                
                for(int ii = expandedValueToken.start; ii < expandedValueToken.end; ii++) {
                    modifier.insertChar(inputParser.getBuffer()[ii]);
                }

                modifier.finish();

                updatedKeys.push_back(key);
            }
                
        }
    }

    if (!updatedKeys.empty()) {
        for(auto it = updatedKeys.begin(); it != updatedKeys.end(); ++it) {
            settingChangeFunctions.forEach(*it);
        }

        save();
    }


    return true;
}


bool SleepHelper::SettingsFile::getValuesJson(String &json) {
    WITH_LOCK(*this) {
        // This annoying code is necessary because String does not have a method to set the
        // string by pointer and length
        json = "";
        size_t size = parser.getOffset();
        json.reserve(size);
        for(size_t ii = 0; ii < size; ii++) {
            char ch = parser.getBuffer()[ii];
            json.concat(ch);
        }
    }

    return true;
}


//
// SettingsFile
//

bool SleepHelper::PersistentData::load() {
    WITH_LOCK(*this) {
        bool loaded = false;

        int dataSize = 0;

        int fd = open(path, O_RDONLY);
        if (fd != -1) {
            dataSize = read(fd, &savedData, sizeof(savedData));
            if (dataSize >= 12 && 
                savedData.magic == SAVED_DATA_MAGIC && 
                savedData.version == SAVED_DATA_VERSION &&
                savedData.size <= dataSize) {                
                if (dataSize < sizeof(savedData)) {
                    // Structure is larger than what's in the file; pad with zero bytes
                    uint8_t *p = (uint8_t *)&savedData;
                    for(size_t ii = (size_t)dataSize; ii < sizeof(savedData); ii++) {
                        p[ii] = 0;
                    }
                }
                savedData.size = (uint16_t) sizeof(savedData);
                loaded = true;
            }
            close(fd);
        }
        
        if (!loaded) {
            memset(&savedData, 0, sizeof(savedData));
            savedData.magic = SAVED_DATA_MAGIC;
            savedData.version = SAVED_DATA_VERSION;
            savedData.size = (uint16_t) sizeof(savedData);
        }
    }

    return true;
}

bool SleepHelper::PersistentData::save() {
    WITH_LOCK(*this) {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
        if (fd != -1) {            
            write(fd, &savedData, sizeof(savedData));
            close(fd);
        }
        else {            
            return false;
        }
    }

    return true;
}

