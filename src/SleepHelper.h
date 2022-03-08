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

    class SleepTimeRecommendation {
    public:
        bool abortSleep = false;
        long minSleepMs = 0;
        long maxSleepMs = 0;
    };


    template<class... Types>
    class AppCallback {
    public:
        void add(std::function<bool(Types... args)> callback) {
            callbackFunctions.push_back(callback);
        }

        void forEach(Types... args) {
            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                (*it)(args...);
            }
        }
        bool untilTrue(bool defaultResult, Types... args) {
            bool res = defaultResult;
            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                res = (*it)(args...);
                if (res) {
                    break;
                }
            }
            return res;
        }

        bool untilFalse(bool defaultResult, Types... args) {
            bool res = defaultResult;
            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                res = (*it)(args...);
                if (!res) {
                    break;
                }
            }
            return res;
        }

        std::vector<std::function<bool(Types... args)>> callbackFunctions;
    };

    class ShouldConnectAppCallback : public AppCallback<bool&, int&> {
    public:
        bool shouldConnect() {
            bool connect;
            int conviction;
            int maxConnectConviction = 0;
            int maxNoConnectConviction = 0;

            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                (*it)(connect, conviction);
                if (connect) {
                    if (conviction > maxConnectConviction) {
                        conviction = maxConnectConviction;
                    }
                }
                else {
                    if (conviction > maxNoConnectConviction) {
                        conviction = maxNoConnectConviction;
                    }
                }
            }

            return (maxConnectConviction >= maxNoConnectConviction);
        }
    };


    SleepHelper &withSetupFunction(std::function<bool()> fn) { 
        setupFunctions.add(fn);
        return *this;
    }

    SleepHelper &withLoopFunction(std::function<bool()> fn) { 
        loopFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withSleepConfigurationFunction(std::function<bool(SystemSleepConfiguration &)> fn) { 
        sleepConfigurationFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withSleepReadyFunction(std::function<bool()> fn) {
        sleepReadyFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withShouldConnectFunction(std::function<bool(bool &connect, int &conviction)> fn) { 
        shouldConnectFunctions.add(fn); 
        return *this; 
    }

    SleepHelper &withMaximumTimeToConnectFunction(std::function<bool(unsigned long ms)> fn) {
        maximumTimeToConnectFunctions.add(fn); 
        return *this; 
    }

    SleepHelper &withMaximumTimeToConnect(std::chrono::milliseconds timeMs) { 
        return withMaximumTimeToConnectFunction([timeMs](unsigned long ms) {
            return (ms >= timeMs.count());
        }); 
    }


    SleepHelper &withMinimumConnectedTimeFunction(std::function<bool(unsigned long ms)> fn) {
        minimumConnectedTimeFunctions.add(fn); 
        return *this; 
    }

    SleepHelper &withMinimumConnectedTime(std::chrono::milliseconds timeMs) { 
        return withMinimumConnectedTimeFunction([timeMs](unsigned long ms) {
            return (ms >= timeMs.count());
        }); 
    }

    /*
    SleepHelper &withSleepTimeRecommendationFunction(std::function<bool(SleepTimeRecommendation &)> fn) { 
        sleepTimeRecommendationFunctions.add(fn); 
        return *this; 
    }
    */

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

    bool reachedMaximumTimeToConnect(long timeMs) {
        return maximumTimeToConnectFunctions.untilFalse(false, timeMs);
    }

    bool reachedMinimumConnectedTime(long timeMs) {
        return minimumConnectedTimeFunctions.untilFalse(false, timeMs);
    }


    void stateHandlerStart();

    void stateHandlerConnectWait();

    void stateHandlerConnected();

    void stateHandlerReconnectWait();

    void stateHandlerPrepareToSleep();

    AppCallback<> setupFunctions;

    AppCallback<> loopFunctions;

    AppCallback<SystemSleepConfiguration &> sleepConfigurationFunctions;

    AppCallback<> sleepReadyFunctions;

    ShouldConnectAppCallback shouldConnectFunctions;

    AppCallback<unsigned long> maximumTimeToConnectFunctions;

    AppCallback<unsigned long> minimumConnectedTimeFunctions;




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
