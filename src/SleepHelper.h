#ifndef __SLEEPHELPER_H
#define __SLEEPHELPER_H

#include "Particle.h"

#include <vector>

/**
 * This class is a singleton; you do not create one as a global, on the stack, or with new.
 * 
 * From global application setup you must call:
 * SleepHelper::instance().setup();
 * 
 * From global application loop you must call:
 * SleepHelper::instance().loop();
 */
class SleepHelper {
public:
    /**
     * @brief Gets the singleton instance of this class, allocating it if necessary
     * 
     * Use SleepHelper::instance() to instantiate the singleton.
     */
    static SleepHelper &instance();

    enum class ShouldConnectResult {
        FORCE_CONNECT,
        IF_TIME,
        FORCE_NO_CONNECT
    };

    class SleepTimeRecommendation {
    public:
        bool abortSleep = false;
        long minSleepMs = 0;
        long maxSleepMs = 0;
    };


    SleepHelper &withSetupFunction(std::function<void(SleepHelper&)> fn) { 
        setupFunctions.push_back(fn);
        return *this;
    }

    SleepHelper &withLoopFunction(std::function<void(SleepHelper&)> fn) { 
        loopFunctions.push_back(fn); 
        return *this;
    }

    SleepHelper &withSleepConfigurationFunction(std::function<void(SleepHelper&, SystemSleepConfiguration &)> fn) { 
        sleepConfigurationFunctions.push_back(fn); 
        return *this;
    }

    SleepHelper &withSleepReadyFunction(std::function<bool(SleepHelper&)> fn) {
        sleepReadyFunctions.push_back(fn); 
        return *this;
    }

    SleepHelper &withShouldConnectFunction(std::function<ShouldConnectResult(SleepHelper&)> fn) { 
        shouldConnectFunctions.push_back(fn); 
        return *this; 
    }

    SleepHelper &withMaximumTimeToConnectFunction(std::function<bool(SleepHelper&, unsigned long ms)> fn) {
        maximumTimeToConnectFunctions.push_back(fn); 
        return *this; 
    }

    SleepHelper &withMaximumTimeToConnect(std::chrono::milliseconds timeMs) { 
        return withMaximumTimeToConnectFunction([timeMs](SleepHelper&, unsigned long ms) {
            return (ms >= timeMs.count());
        }); 
    }


    SleepHelper &withMinimumConnectedTimeFunction(std::function<bool(SleepHelper&, unsigned long ms)> fn) {
        minimumConnectedTimeFunctions.push_back(fn); 
        return *this; 
    }

    SleepHelper &withMinimumConnectedTime(std::chrono::milliseconds timeMs) { 
        return withMinimumConnectedTimeFunction([timeMs](SleepHelper&, unsigned long ms) {
            return (ms >= timeMs.count());
        }); 
    }


    /**
     * @brief Perform setup operations; call this from global application setup()
     * 
     * You typically use SleepHelper::instance().setup();
     */
    void setup();

    /**
     * @brief Perform application loop operations; call this from global application loop()
     * 
     * You typically use SleepHelper::instance().loop();
     */
    void loop();

    /**
     * @brief Determine if it's a good time to go to sleep
     * 
     * The sleep ready functions registered with withSleepReadyFunction() are called. If any returns false (not ready)
     * then this function returns false. If all return true (or there are no sleep ready functions) then this function
     * returns true.
     */
    bool isSleepReady();

    bool shouldConnect();

protected:
    /**
     * @brief The constructor is protected because the class is a singleton
     * 
     * Use SleepHelper::instance() to instantiate the singleton.
     */
    SleepHelper();

    /**
     * @brief The destructor is protected because the class is a singleton and cannot be deleted
     */
    virtual ~SleepHelper();

    /**
     * This class is a singleton and cannot be copied
     */
    SleepHelper(const SleepHelper&) = delete;

    /**
     * This class is a singleton and cannot be copied
     */
    SleepHelper& operator=(const SleepHelper&) = delete;


    bool reachedMaximumTimeToConnect(long timeMs);

    bool reachedMinimumConnectedTime(long timeMs);


    void stateHandlerStart();

    void stateHandlerConnectWait();

    void stateHandlerConnected();

    void stateHandlerReconnectWait();

    void stateHandlerPrepareToSleep();

    std::vector<std::function<void(SleepHelper&)>> setupFunctions;

    std::vector<std::function<void(SleepHelper&)>> loopFunctions;

    std::vector<std::function<void(SleepHelper&, SystemSleepConfiguration &)>> sleepConfigurationFunctions;

    std::vector<std::function<bool(SleepHelper&)>> sleepReadyFunctions;

    std::vector<std::function<ShouldConnectResult(SleepHelper&)>> shouldConnectFunctions;

    std::vector<std::function<bool(SleepHelper&, unsigned long ms)>> maximumTimeToConnectFunctions;

    std::vector<std::function<bool(SleepHelper&, unsigned long ms)>> minimumConnectedTimeFunctions;


    std::function<void(SleepHelper&)> stateHandler = &SleepHelper::stateHandlerStart;

    unsigned long connectAttemptStartMillis = 0;
    unsigned long connectedStartMillis = 0;

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static SleepHelper *_instance;

};
#endif  /* __SLEEPHELPER_H */
