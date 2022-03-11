#ifndef __SLEEPHELPER_H
#define __SLEEPHELPER_H

#include "Particle.h"
#include "LocalTimeRK.h"
#include "JsonParserGeneratorRK.h"
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

        bool whileAnyTrue(bool defaultResult, Types... args) {
            bool finalRes = defaultResult;

            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                bool res = (*it)(args...);
                if (res) {
                    finalRes = true;
                }
            }
            return finalRes;
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

        bool whileAnyFalse(bool defaultResult, Types... args) {
            bool finalRes = defaultResult;
            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                bool res = (*it)(args...);
                if (!res) {
                    finalRes = res;
                }
            }
            return finalRes;
        }
        std::vector<std::function<bool(Types... args)>> callbackFunctions;
    };

    class ShouldConnectAppCallback : public AppCallback<int&, int&> {
    public:
        bool shouldConnect() {
            int maxConnectConviction = 0;
            int maxNoConnectConviction = 0;

            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                int connectConviction = 0;
                int noConnectConviction = 0;
                (*it)(connectConviction, noConnectConviction);
                if (connectConviction > maxConnectConviction) {
                    maxConnectConviction = connectConviction;
                }
                if (noConnectConviction > maxNoConnectConviction) {
                    maxNoConnectConviction = noConnectConviction;
                }
            }

            return (maxConnectConviction >= maxNoConnectConviction);
        }
    };

    /**
     * @brief Class for managing the SleepHelper settings file
     * 
     * You must not access the settings file at global constructor time. Only use it from
     * setup() or later. You can access it from worker threads.
     */
    class SettingsFile : public Mutex {
    public:
        /**
         * @brief Get the singleton instance of the SettingsFile class. Do not call at global construction time!
         * 
         * @return SettingsFile& 
         */
        static SettingsFile &instance();

        SettingsFile &withSettingChangeFunction(std::function<bool(const char *)> fn) { 
            settingChangeFunctions.add(fn);
            return *this;
        }


        /**
         * @brief Load the settings file. You normally do not need to call this; it will be loaded automatically.
         * 
         * @return true 
         * @return false 
         */
        bool load();

        /**
         * @brief Save the settings file. You normally do not need to call this; it will be saved automatically.
         * 
         * @return true 
         * @return false 
         */
        bool save();

    	template<class T>
	    bool getValue(const char *name, T &value) const {
            bool result = false;
            WITH_LOCK(*this) {
                result = parser.getOuterValueByKey(name, result);
            };
            return result;
        }

        /**
         * @brief Sets the value of a key to a bool, int, double, or String value
         * 
         * @tparam T 
         * @param name 
         * @param value 
         * @return true 
         * @return false
         * 
         * This call returns quickly if the value does not change and does not write to
         * the file system if the value is unchanged.
         */
    	template<class T>
	    bool setValue(const char *name, const T &value) {
            bool result = true;
            WITH_LOCK(*this) {
                T oldValue;
                bool getResult = parser.getOuterValueByKey(name, oldValue);
                if (!getResult || oldValue != value) {
                    JsonModifier modifier(parser);

                    modifier.insertOrUpdateKeyValue(parser.getOuterObject(), name, value);
                }
                else {
                    printf("not changed\n");
                }

            };
            return result;
        }

        /**
         * @brief Set the value of a key to a constant string
         * 
         * @param name 
         * @param value 
         * @return true 
         * @return false 
         * 
         * This call returns quickly if the value does not change.
         * 
         * This specific overload is required because the templated version above can't get the
         * value to see if it changed into a const char *. Making a copy of it in a string
         * solves this issue.
         */
	    bool setValue(const char *name, const char *value) {
            String tempStr(value);

            return setValue(name, tempStr);
        }

        bool setValuesJson(const char *json);


        static constexpr const char * const SETTINGS_PATH = 
#ifndef UNITTEST
            "/usr/sleepSettings.json";
#else
            "./sleepSettings.json";
#endif

    protected:
        /**
         * @brief The constructor is protected because the class is a singleton
         * 
         * Use SleepHelper::SettingsFile::instance() to instantiate the singleton.
         */
        SettingsFile() {};

        /**
         * @brief The destructor is protected because the class is a singleton and cannot be deleted
         */
        virtual ~SettingsFile() {};

        /**
         * This class is a singleton and cannot be copied
         */
        SettingsFile(const SettingsFile&) = delete;

        /**
         * This class is a singleton and cannot be copied
         */
        SettingsFile& operator=(const SettingsFile&) = delete;

        JsonParserStatic<particle::protocol::MAX_EVENT_DATA_LENGTH, 50> parser;

        AppCallback<const char *> settingChangeFunctions;

        static SettingsFile *_settingsFile;
    };


#ifndef UNITTEST
    SleepHelper &withSetupFunction(std::function<bool()> fn) { 
        setupFunctions.add(fn);
        return *this;
    }

    SleepHelper &withLoopFunction(std::function<bool()> fn) { 
        loopFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withSleepConfigurationFunction(std::function<bool(SystemSleepConfiguration &, std::chrono::milliseconds&)> fn) { 
        sleepConfigurationFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withSleepReadyFunction(std::function<bool(system_tick_t)> fn) {
        sleepReadyFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withShouldConnectFunction(std::function<bool(int &connectConviction, int &noConnectConviction)> fn) { 
        shouldConnectFunctions.add(fn); 
        return *this; 
    }

    SleepHelper &withWakeFunction(std::function<bool(const SystemSleepResult &)> fn) { 
        wakeFunctions.add(fn); 
        return *this;
    }


    SleepHelper &withWakeOrBootFunction(std::function<bool()> fn) { 
        wakeOrBootFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Adds a function to be called right before sleep or reset.
     * 
     * @param fn A function or lambda to call. The bool result is ignored.
     * 
     * @return SleepHelper& 
     * 
     * It's common to put code to power down peripherals and stop an external hardware watchdog.
     * This should only be used for quick operations. You will already be disconnected from the
     * cloud and network when this function is called. You cannot stop the reset or sleep process
     * from this callback.
     * 
     * The order of callbacks is sleepOrReset, sleepConfiguration, then the device goes to sleep.
     * When the device wakes, the wake callback is called.
     */
    SleepHelper &withSleepOrResetFunction(std::function<bool(bool)> fn) { 
        sleepOrResetFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withMaximumTimeToConnectFunction(std::function<bool(system_tick_t ms)> fn) {
        maximumTimeToConnectFunctions.add(fn); 
        return *this; 
    }

    SleepHelper &withMaximumTimeToConnect(std::chrono::milliseconds timeMs) { 
        return withMaximumTimeToConnectFunction([timeMs](system_tick_t ms) {
            return (ms >= timeMs.count());
        }); 
    }

    /**
     * @brief Register a callback for when in the no connection state
     * 
     * If the should connect handlers indicate that a cloud connection should not be attempted,
     * for example if we're in a situation we wake briefly to sample a sensor but want to 
     * aggregate values before connecting to the cloud to avoid connecting too frequently, 
     * then the no connect handler provides time for your application to do thing before
     * going to sleep. If you return true from your callback, then the device will continue
     * to stay awake. 
     * 
     * @param fn 
     * @return SleepHelper& 
     */
    SleepHelper &withNoConnectionFunction(std::function<bool()> fn) {
        noConnectionFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withWhileConnectedFunction(std::function<bool()> fn) {
        whileConnectedFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withMinimumConnectedTime(std::chrono::milliseconds timeMs) { 
        return withSleepReadyFunction([timeMs](system_tick_t ms) {
            return (ms >= timeMs.count());
        }); 
    }


    
#if HAL_PLATFORM_POWER_MANAGEMENT
    /**
     * @brief Require a minimum battery SoC to connect to cellular
     * 
     * @param minSoC The minimum required in the range of 0.0 to 100.0
     * @param conviction The noConnect conviction level to use. If this is greater than the connection
     * conviction form other should connect functions, then connection will be prevented. Default: 100
     * 
     * @return SleepHelper& 
     */
    SleepHelper &withShouldConnectMinimumSoC(float minSoC, int conviction = 100) {
        return withShouldConnectFunction([minSoC, conviction](int &connectConviction, int &noConnectConviction) {            
            float soc = System.batteryCharge();
            if (soc >= 0 && soc < minSoC) {
                // Battery is known and too low to connect
                noConnectConviction = conviction;
            }
            return true;
        });
    }
#else
    SleepHelper &withMinimumSoC(float minSoC, int conviction = 100) {
        return *this;
    }
#endif

#ifdef __PUBLISHQUEUEPOSIXRK_H
    SleepHelper &withPublishQueuePosixRK(std::chrono::milliseconds maxTimeToPublish = 0ms) {
        return withSleepReadyFunction([maxTimeToPublish](system_tick_t ms) {
            if (maxTimeToPublish.count() != 0 && ms >= maxTimeToPublish.count()) {
                PublishQueuePosix::instance()::setPausePublishing(true);
            }
            bool canSleep = PublishQueuePosix::instance()::getCanSleep();
            if (canSleep) {
                PublishQueuePosix::instance()::pausePublishing();
                PublishQueuePosix::instance()::writeQueueToFiles();
            }
            return canSleep;
        })
    }
#endif

    /**
     * @brief Sets the time configuration string for local time calculations
     * 
     * @param tzConfig Timezone and daylight saving settings, POSIX timezone rule string
     * @return SleepHelper& 
     * 
     * If you do not call this, all time calculation will be done at UTC.
     * 
     * For the United States east coast, the configuration string is:
     * 
     * ```
     * EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00"
     * ```
     * 
     * What this means is:
     * 
     * - "EST" is the standard time timezone name
     * - 5 is the offset from UTC in hours. Actually -5, as the sign is backwards from the way it's normally expressed. And it could be hours and minutes.
     * - "EDT" is the daylight saving time timezone name
     * - "M3.2.0" is when DST starts. Month 3 (March), 2nd week, 0 = Sunday
     * - "2:00:00" transition to DST at 2:00 AM local time
     * - "M11.1.0" transition back to standard time November, 1st week, on Sunday
     * - "2:00:00" transition back to standard time at 2:00 AM local time
     * 
     * Some examples:
     * 
     * | Location            | Timezone Configuration |
     * | :------------------ | :--- |
     * | New York            | "EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00" |
     * | Chicago             | "CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00" |
     * | Denver              | "MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00" |
     * | Phoenix             | "MST7" |
     * | Los Angeles         | "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00" |
     * | London              | "BST0GMT,M3.5.0/1:00:00,M10.5.0/2:00:00" |
     * | Sydney, Australia   | "AEST-10AEDT,M10.1.0/02:00:00,M4.1.0/03:00:00" | 
     * | Adelaide, Australia | "ACST-9:30ACDT,M10.1.0/02:00:00,M4.1.0/03:00:00" |
     * 
     */
    SleepHelper &withTimeConfig(const char *tzConfig) {
        LocalTime::instance().withConfig(tzConfig);
        return *this;
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

#endif // UNITTEST


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

#ifndef UNITTEST
    void systemEventHandler(system_event_t event, int param);

    static void systemEventHandlerStatic(system_event_t event, int param);

    void stateHandlerStart();

    void stateHandlerConnectWait();

    void stateHandlerConnected();

    void stateHandlerReconnectWait();

    void stateHandlerNoConnection();

    void stateHandlerDisconnectBeforeSleep();

    void stateHandlerPrepareToSleep();


    AppCallback<> setupFunctions;

    AppCallback<> loopFunctions;

    AppCallback<SystemSleepConfiguration &, std::chrono::milliseconds&> sleepConfigurationFunctions;

    AppCallback<system_tick_t> sleepReadyFunctions;

    ShouldConnectAppCallback shouldConnectFunctions;

    AppCallback<const SystemSleepResult &> wakeFunctions;

    AppCallback<> wakeOrBootFunctions;

    AppCallback<bool> sleepOrResetFunctions;

    AppCallback<system_tick_t> maximumTimeToConnectFunctions;

    AppCallback<> noConnectionFunctions;

    AppCallback<> whileConnectedFunctions;

    LocalTimeConvert::Schedule sleepSchedule;
    LocalTimeConvert::Schedule publishSchedule;

    std::function<void(SleepHelper&)> stateHandler = &SleepHelper::stateHandlerStart;

    system_tick_t connectAttemptStartMillis = 0;
    system_tick_t connectedStartMillis = 0;
    bool outOfMemory = false;
#endif // UNITTEST

    Logger appLog;

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static SleepHelper *_instance;

};
#endif  /* __SLEEPHELPER_H */
