#include "SleepHelper.h"

#ifndef UNITTEST
#include "BackgroundPublishRK.h"
#endif

#include <fcntl.h>
#include <algorithm> // std::sort

SleepHelper *SleepHelper::_instance;

// [static]
SleepHelper &SleepHelper::instance() {
    if (!_instance) {
        _instance = new SleepHelper();
    }
    return *_instance;
}

SleepHelper::SleepHelper() : appLog("app.sleep") {
    
    settingsFile.withPath("/usr/sleepSettings.json");

    persistentData.withPath("/usr/sleepData.dat");
}

SleepHelper::~SleepHelper() {
}

#ifndef UNITTEST
void SleepHelper::setup() {
    // Register for system events
    System.on(firmware_update | firmware_update_pending | reset | out_of_memory, systemEventHandlerStatic);

    settingsFile.setup();
    persistentData.setup();

    // This library directly uses BackgroundPublishRK to publish from a worker thread to 
    // avoid blocking. You can safely use this at the same time as using 
    // PublishQueuePosixRK to handle publishing with saving publishes to
    // the flash file system for publishing later.
	BackgroundPublishRK::instance().start();

    // Call all setup functions
    setupFunctions.forEach();

    // Called from setup(), and also after waking from sleep
    wakeOrBootFunctions.forEach();

    // Always wait until we have a valid RTC time before sleeping if cloud connected
    withSleepReadyFunction([](system_tick_t) {
        // Return true if it's OK to sleep or false if not.
        return Time.isValid();
    });

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
    networkConnectedMillis = 0;
}


void SleepHelper::stateHandlerConnectWait() {
    if (Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerTimeValidWait;
        return;
    }
    if (!networkConnectedMillis && Cellular.connected()) {
        networkConnectedMillis = millis();

        system_tick_t elapsedMs = networkConnectedMillis - connectAttemptStartMillis;
        appLog.info("connected to network in %lu ms", elaposedMs);
    }

    system_tick_t elapsedMs = millis() - connectAttemptStartMillis;

    if (maximumTimeToConnectFunctions.whileAnyFalse(false, elapsedMs)) {
        appLog.info("timed out connecting to cloud");
        stateHandler = &SleepHelper::stateHandlerDisconnectBeforeSleep;
        return;
    }

}

void SleepHelper::stateHandlerTimeValidWait() {
    // Wait until we get a valid RTC clock time. This happens immediately after 
    // connecting to the cloud, and will likely already be set on wake from
    // sleep, so this will be instantaneous in many cases.
    if (Time.isValid()) {
        stateHandler = &SleepHelper::stateHandlerConnectedStart;
        return;
    }
}


void SleepHelper::stateHandlerConnectedStart() {
    connectedStartMillis = millis();

    system_tick_t elapsedMs = connectedStartMillis - connectAttemptStartMillis;
    appLog.info("connected to cloud in %lu ms", elapsedMs);

    if (wakeEventName.length() > 0) {
        // Call the wake event handlers to see if they have JSON data to publish
        std::vector<String> events;
        wakeEventFunctions.generateEvents(events);

        // If there are events, add to the publish queue
        for(auto it = events.begin(); it != events.end(); ++it) {
            publishData.push_back(PublishData(wakeEventName, *it));            
        }
    }


    stateHandler = &SleepHelper::stateHandlerConnected;
}

void SleepHelper::stateHandlerConnected() {
    if (!Particle.connected()) {
        stateHandler = &SleepHelper::stateHandlerReconnectWait;
        return;        
    }

    if (!publishData.empty()) {
        PublishData event = publishData.front();

        stateTime = millis();

        // TODO: Pause PublishQueuePosixRK processing until our immediate events are finished

        stateHandler = &SleepHelper::stateHandlerPublishWait;

        bool bResult = BackgroundPublishRK::instance().publish(event.eventName, event.eventData, event.flags, 
            [this](bool succeeded, const char *event_name, const char *event_data, const void *event_context) {
            // Callback
            if (succeeded) {
                appLog.info("removing item from publishData");
                publishData.erase(publishData.begin());
            }
            stateHandler = &SleepHelper::stateHandlerPublishRateLimit;
        });
        if (!bResult) {
            stateHandler = &SleepHelper::stateHandlerConnected;
        }
        return;
    }


    system_tick_t elapsedMs = millis() - connectedStartMillis;
    if (sleepReadyFunctions.whileAnyFalse(true, elapsedMs)) {
        // Ready to sleep, go into prepare to sleep state
        stateHandler = &SleepHelper::stateHandlerDisconnectBeforeSleep;
        return;
    }
    

}

void SleepHelper::stateHandlerPublishWait() {
    // Exiting this state happens from the background publish callback lambda, see stateHandlerConnected state
}

void SleepHelper::stateHandlerPublishRateLimit() {
    if (millis() - stateTime > 1000) {
        stateHandler = &SleepHelper::stateHandlerConnected;
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

void SleepHelper::SettingsFile::setup() {
}

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

    // Merge in any default values
    if (defaultValues) {
        addDefaultValues(defaultValues);
    }

    return true;
}

bool SleepHelper::SettingsFile::save() {
    WITH_LOCK(*this) {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
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

            // Does this item exist?
    		const JsonParserGeneratorRK::jsmntok_t *oldValueToken;
            if (!parser.getValueTokenByKey(parser.getOuterObject(), key, oldValueToken)) {
                // Key does not exist, issue a change notification
                updatedKeys.push_back(key);
            }
            else {
                int valueLen = valueToken->end - valueToken->start;
                int oldValueLen = oldValueToken->end - oldValueToken->start;

                if (valueToken->type != oldValueToken->type || 
                    valueLen != oldValueLen ||
                    memcmp(inputParser.getBuffer() + valueToken->start, parser.getBuffer() + oldValueToken->start, valueLen) != 0) {

                    // Changed value
                    updatedKeys.push_back(key);
                }
            }


                
        }
    }

    if (!updatedKeys.empty()) {
        for(auto it = updatedKeys.begin(); it != updatedKeys.end(); ++it) {
            settingChangeFunctions.forEach(*it);
        }

        // Replace existing settings
        parser.clear();
        parser.addString(inputJson);
        parser.parse();

        save();
    }


    return true;
}



bool SleepHelper::SettingsFile::updateValuesJson(const char *inputJson) {
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

bool SleepHelper::SettingsFile::addDefaultValues(const char *inputJson) {
    bool needsSave = false;

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

                // Update the inserted token to be the actual data to insert
                parser.getValueTokenByKey(parser.getOuterObject(), key, oldValueToken);
                const JsonParserGeneratorRK::jsmntok_t expandedValueToken = modifier.tokenWithQuotes(valueToken);
                const JsonParserGeneratorRK::jsmntok_t expandedOldValueToken = modifier.tokenWithQuotes(oldValueToken);
                modifier.startModify(&expandedOldValueToken);
                
                for(int ii = expandedValueToken.start; ii < expandedValueToken.end; ii++) {
                    modifier.insertChar(inputParser.getBuffer()[ii]);
                }

                modifier.finish();
                needsSave = true;
            }                
        }
    }

    if (needsSave) {
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
// SleepHelper::CloudSettingsFile
//
uint32_t SleepHelper::CloudSettingsFile::getHash() const {
    uint32_t hash;

    WITH_LOCK(*this) {
        hash = murmur3_32((const uint8_t *)parser.getBuffer(), parser.getOffset(), HASH_SEED);
    }

    return hash;
}


uint32_t SleepHelper::CloudSettingsFile::murmur3_32(const uint8_t* key, size_t len, uint32_t seed) {
    // https://en.wikipedia.org/wiki/MurmurHash
	uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmur_32_scramble(k);
    /* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}


//
// PersistentDataBase
//

void SleepHelper::PersistentDataBase::setup() {
    // Load data at boot
    load();
}

bool SleepHelper::PersistentDataBase::load() {
    WITH_LOCK(*this) {
        if (!validate(savedDataSize)) {
            initialize();
        }
    }

    return true;
}



bool SleepHelper::PersistentDataBase::getValueString(size_t offset, size_t size, String &value) const {
    bool result = false;

    WITH_LOCK(*this) {
        if (offset <= (savedDataSize - (size - 1))) {
            const char *p = (const char *)savedDataHeader;
            p += offset;
            value = p; // copies string
            result = true;
        }
    }
    return result;
}

bool SleepHelper::PersistentDataBase::setValueString(size_t offset, size_t size, const char *value) {
    bool result = false;

    WITH_LOCK(*this) {
        if (offset <= (savedDataSize - (size - 1)) && strlen(value) < size) {
            char *p = (char *)savedDataHeader;
            p += offset;

            if (strcmp(value, p) != 0) {
                memset(p, 0, size);
                strcpy(p, value);
                saveOrDefer();
            }
            result = true;
        }
    }
    return result;
}

bool SleepHelper::PersistentDataBase::validate(size_t dataSize) {
    bool isValid = false;

    if (dataSize >= 12 && 
        savedDataHeader->magic == savedDataMagic && 
        savedDataHeader->version == savedDataVersion &&
        savedDataHeader->size <= (uint16_t) dataSize) {                
        if ((size_t)dataSize < savedDataSize) {
            // Structure is larger than what's in the file; pad with zero bytes
            uint8_t *p = (uint8_t *)savedDataHeader;
            for(size_t ii = (size_t)dataSize; ii < savedDataSize; ii++) {
                p[ii] = 0;
            }
        }
        savedDataHeader->size = (uint16_t) savedDataSize;
        isValid = true;
    }   
    return isValid;
}

void SleepHelper::PersistentDataBase::initialize() {
    memset(savedDataHeader, 0, savedDataSize);
    savedDataHeader->magic = savedDataMagic;
    savedDataHeader->version = savedDataVersion;
    savedDataHeader->size = (uint16_t) savedDataSize;
}

//
// PersistentDataFile
//

void SleepHelper::PersistentDataFile::setup() {
    // Call parent class
    SleepHelper::PersistentDataBase::setup();

    SleepHelper::instance().withLoopFunction([this]() {
        // Handle deferred save
        flush(false);
        return true;
    });
    SleepHelper::instance().withSleepOrResetFunction([this](bool) {
        // Make sure data is saved before sleep or reset
        flush(true);
        return true;
    });
}

bool SleepHelper::PersistentDataFile::load() {
    WITH_LOCK(*this) {
        bool loaded = false;

        int dataSize = 0;

        int fd = open(path, O_RDONLY);
        if (fd != -1) {
            dataSize = read(fd, savedDataHeader, savedDataSize);
            if (validate(dataSize)) {
                loaded = true;
            }

            close(fd);
        }
        
        if (!loaded) {
            initialize();
        }
    }

    return true;
}



void SleepHelper::PersistentDataFile::save() {
    WITH_LOCK(*this) {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd != -1) {            
            write(fd, savedDataHeader, savedDataSize);
            close(fd);
        }
    }
}

void SleepHelper::PersistentDataFile::flush(bool force) {
    if (lastUpdate) {
        if (force || (millis() - lastUpdate >= saveDelayMs)) {
            save();
            lastUpdate = 0;
        }
    }
}

//
// EventCombiner
//
void SleepHelper::EventCombiner::generateEvents(std::vector<String> &events) {
    size_t maxSize = particle::protocol::MAX_EVENT_DATA_LENGTH;

    // TODO: Also use Particle.maxEventDataSize() (available in 3.1 and later) to limit event size
    generateEvents(events, maxSize);
}

static bool _keyCompare(const String &a, const String &b) {
    return strcmp(a, b) > 0;
}

void SleepHelper::EventCombiner::generateEvents(std::vector<String> &events, size_t maxSize) {
    
    events.clear();

    std::vector<EventInfo> infoArray;
    char *buf = (char *)malloc(maxSize + 1);
    if (!buf) {
        return;
    }

    for(auto it = callbacks.callbackFunctions.begin(); it != callbacks.callbackFunctions.end(); ++it) {
        generateEventInternal(*it, buf, maxSize, infoArray);        
    }
    for(auto it = oneTimeCallbacks.callbackFunctions.begin(); it != oneTimeCallbacks.callbackFunctions.end(); ++it) {
        generateEventInternal(*it, buf, maxSize, infoArray);        
    }

    if (!infoArray.empty()) {
        // Sort highest priority first
        std::sort(infoArray.begin(), infoArray.end(), [](EventInfo a, EventInfo b) {
            return a.priority > b.priority;
        });

        // Dedupe keys in case a one-time callback is called more than once
        std::vector<String> keysAdded;
        for(auto it = infoArray.begin(); it != infoArray.end(); ) {
            bool keyExists = false;
            
            for(auto it2 = it->keys.begin(); it2 != it->keys.end(); ++it2) {
                for(auto it3 = keysAdded.begin(); it3 != keysAdded.end(); ++it3) {
                    if (*it3 == *it2) {
                        keyExists = true;
                        break;
                    }
                }
                keysAdded.push_back(*it2);
            }

            if (keyExists) {
                it = infoArray.erase(it);
            }
            else {
                ++it;
            }
        }

        // 
        char *cur = buf;
        char *end = &buf[maxSize - 2]; // Room for leading , and trailing }

        *cur++ = '{';
        bool firstEventBuffer = true;

        for(auto it = infoArray.begin(); it != infoArray.end(); ++it) {
            
            if (&cur[strlen(it->json)] >= end) {
                // Buffer is full
                if (cur > &buf[1]) {
                    *cur++ = '}';
                    *cur = 0;
                    events.push_back(buf);
                    cur = &buf[1];
                }
                firstEventBuffer = false;
            }

            if (!firstEventBuffer && it->priority < 50) {
                break;
            }        

            if (cur != &buf[1]) {
                *cur++ = ',';
            }

            strcpy(cur, it->json);
            cur += strlen(cur);
        }

        if (cur > &buf[1]) {
            // Write out last object
            *cur++ = '}';
            *cur = 0;
            events.push_back(buf);
        }
    }

    clearOneTimeCallbacks();

    free(buf);
}


void SleepHelper::EventCombiner::generateEventInternal(std::function<void(JSONWriter &, int &)> callback, char *buf, size_t maxSize, std::vector<EventInfo> &infoArray) {
    memset(buf, 0, maxSize);
    JSONBufferWriter writer(buf, maxSize);

    int priority = 0;

    writer.beginObject();
    callback(writer, priority);
    writer.endObject();

    if (priority > 0 && strlen(buf) > 2) {
        // Priority is set and not an empty object
        if (writer.dataSize() <= writer.bufferSize()) {
            // Callback data was not truncated 

            EventInfo eventInfo;
            eventInfo.priority = priority;

            // Gather keys used in this
            JSONValue outerObj = JSONValue::parseCopy(buf);
            JSONObjectIterator iter(outerObj);
            while(iter.next()) {
                eventInfo.keys.push_back((const char *)iter.name());
            }

            // Remove the } at the end of the object
            buf[strlen(buf) - 1] = 0;
            eventInfo.json = &buf[1];

            infoArray.push_back(eventInfo);
        }
    }
}

