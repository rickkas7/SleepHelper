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

    /**
     * @brief This is a wrapper around a recursive mutex, similar to Device OS RecursiveMutex
     * 
     * There are two differences:
     * 
     * - The mutex is created on first lock, instead of from the constructor. This is done because it's
     * not save to call os_mutex_recursive_create from a global constructor, and by delaying 
     * construction it makes it possible to safely construct the class as a global object.
     * - The lock/trylock/unlock methods are declared const and the mutex handle mutable. This allows the
     * mutex to be locked from a const method.
     */
#ifndef UNITTEST
    class SleepHelperRecursiveMutex
    {
        mutable os_mutex_recursive_t handle_;

    public:
        /**
         * Creates a shared mutex.
         */
        SleepHelperRecursiveMutex(os_mutex_recursive_t handle) : handle_(handle) {
        }

        SleepHelperRecursiveMutex() : handle_(nullptr) {
        }

        ~SleepHelperRecursiveMutex() {
            dispose();
        }

        void dispose() {
            if (handle_) {
                os_mutex_recursive_destroy(handle_);
                handle_ = nullptr;
            }
        }

        void lock() const { 
            if (!handle_) {
                os_mutex_recursive_create(&handle_);
            }
            os_mutex_recursive_lock(handle_); 
        }
        bool trylock() const { 
            if (!handle_) {
                os_mutex_recursive_create(&handle_);
            }
            return os_mutex_recursive_trylock(handle_)==0; 
        }
        bool try_lock() const { 
            return trylock(); 
        }
        void unlock() const { 
            os_mutex_recursive_unlock(handle_); 
        }
    };
#else
    class SleepHelperRecursiveMutex {
    public:
        void lock() const {};
        bool trylock() const { 
            return true;
        }
        bool try_lock() const { 
            return trylock(); 
        }
        void unlock() const { 
        }
    };
#endif

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

        /**
         * @brief Remove all registered callbacks
         * 
         * You normally will never use this. It's used by the automated test suite. There's no function 
         * to remove a single callback since they're typically lambas and it would be difficult to
         * specify which one to remove.
         */
        void removeAll() {
            callbackFunctions.clear();
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
     * 
     * Settings are limited to the size of a publish, function, or variable data payload, 
     * typically 1024 bytes on Gen 3 devices. 
     * 
     * If you have more data than that, you should store it in your own file. You can 
     * also create more than one SettingsFile object for your own settings, but it won't
     * be connected to the built-in function and variable support.
     */
    class SettingsFile : public SleepHelperRecursiveMutex {
    public:
        /**
         * @brief Default constructor. Use withPath() to set the pathname if using this constructor
         */
        SettingsFile() {};

        /**
         * @brief Constructor that thats a pathname to the settings file
         * 
         * @param path 
         */
        SettingsFile(const char *path) : path(path) {};

        /**
         * @brief Destructor
         */
        virtual ~SettingsFile() {};

        /**
         * @brief Sets the path to the settings file on the file system
         * 
         * @param path 
         * @return SettingsFile& 
         */
        SettingsFile &withPath(const char *path) { 
            this->path = path; 
            return *this; 
        };
        
        /**
         * @brief Register a function to be called when a settings value is changed
         * 
         * @param fn a function or lamba to call
         * @return SettingsFile& 
         */
        SettingsFile &withSettingChangeFunction(std::function<bool(const char *)> fn) { 
            settingChangeFunctions.add(fn);
            return *this;
        }
        /**
         * @brief Initialize this object for use in SleepHelper
         * 
         * This is used from SleepHeler::setup(). You should not use this if you are creating your
         * own SettingsFile object; this is only used to hook this class into SleepHelper/
         */
        void setup();

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
        
        /**
         * @brief Get a value from the settings file
         * 
         * @tparam T 
         * @param name 
         * @param value 
         * @return true 
         * @return false 
         * 
         * The values are cached in RAM, so this is normally fast. Note that you must request the same data type as 
         * the original data in the JSON file - it does not coerce types.
         */
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
         * 
         * You should use the same data type as was originally in the JSON data beacause
         * the change detection does not coerce data types.
         */
    	template<class T>
	    bool setValue(const char *name, const T &value) {
            bool result = true;
            bool changed = false;

            WITH_LOCK(*this) {
                T oldValue;
                bool getResult = parser.getOuterValueByKey(name, oldValue);
                if (!getResult || oldValue != value) {
                    JsonModifier modifier(parser);

                    modifier.insertOrUpdateKeyValue(parser.getOuterObject(), name, value);
                    changed = true;
                }

            };

            if (changed) {
                settingChangeFunctions.forEach(name);
                save();
            }
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
        
        /**
         * @brief Merges multiple values from JSON data into the settings
         * 
         * @param json 
         * @return true 
         * @return false 
         * 
         */
        bool setValuesJson(const char *json);

        bool getValuesJson(String &json);

    protected:
        /**
         * This class cannot be copied
         */
        SettingsFile(const SettingsFile&) = delete;

        /**
         * This class cannot be copied
         */
        SettingsFile& operator=(const SettingsFile&) = delete;

        JsonParserStatic<particle::protocol::MAX_EVENT_DATA_LENGTH, 50> parser;

        AppCallback<const char *> settingChangeFunctions;
        String path;
    };


    /**
     * @brief Class for managing small persistent data
     * 
     * You must not access the persistent at global constructor time. Only use it from
     * setup() or later. You can access it from worker threads.
     * 
     */
    class PersistentData : public SleepHelperRecursiveMutex {
    public:
        /**
         * @brief Structure saved to the persistent data file (binary)
         * 
         * You must not change the first two fields (magic, version) and the data
         * structure must always be >= 12 bytes (including size and flags).
         * 
         * You can expand the structure later without incrementing the
         * version number. Added fields will be initialized to 0. The size is 
         * limited to 65536 bytes (uint16_t maximum value).
         * 
         * Since the SavedData structure is always stored in RAM, you should not
         * make it excessively large
         */
        class SavedData {
        public:
            uint32_t magic;                 //!< SAVED_DATA_MAGIC = 0xd87cb6ce;
            uint32_t version;               //!< SAVED_DATA_VERSION = 1
            uint16_t size;                  //!< sizeof(SavedData)
            uint16_t flags;                 //!< currently 0
            uint32_t nextUpdateCheck;
            uint32_t nextPublish;
            uint32_t nextQuickWake;
            // OK to add more fields here later without incremeting version
        };

        /**
         * @brief Default constructor. Use withPath() to set the pathname if using this constructor
         */
        PersistentData() {};

        /**
         * @brief Constructor that thats a pathname to the persistent data file
         * 
         * @param path 
         */
        PersistentData(const char *path) : path(path) {};

        /**
         * @brief Destructor
         */
        virtual ~PersistentData() {};

        /**
         * @brief Sets the path to the persistent data file on the file system
         * 
         * @param path 
         * @return PersistentData& 
         */
        PersistentData &withPath(const char *path) { 
            this->path = path; 
            return *this; 
        };
        
        /**
         * @brief Sets the wait to save delay. Default is 1000 milliseconds.
         * 
         * @param value Value is milliseconds, or 0
         * @return PersistentData& 
         * 
         * Normally, if the value is changed by a set call, then about
         * one second later the change will be saved to disk from the loop thread. The
         * savedData is also saved before sleep or reset if changed.
         * 
         * You can change the save delay by using withSaveDelayMs(). If you set it to 0, then
         * the data is saved within the setValue call immediately, which will make all set calls
         * run more slowly.
         */
        PersistentData &withSaveDelayMs(uint32_t value) {
            saveDelayMs = value;
            return *this;
        }

        /**
         * @brief Initialize this object for use in SleepHelper
         * 
         * This is used from SleepHelper::setup(). You should not use this if you are creating your
         * own PersistentData object; this is only used to hook this class into SleepHelper/
         */
        void setup();

        /**
         * @brief Load the persistent data file. You normally do not need to call this; it will be loaded automatically.
         * 
         * @return true 
         * @return false 
         */
        bool load();

        /**
         * @brief Save the persistent data file. You normally do not need to call this; it will be saved automatically.
         * 
         * @return true 
         * @return false 
         */
        bool save();

        /**
         * @brief Write the settings to disk if changed and the wait to save time has expired
         * 
         * @param force Pass true to ignore the wait to save time and save immediately if necessary. This
         * is used when you're about to sleep or reset, for example.
         * 
         * This call is fast if a save is not required so you can call it frequently, even every loop.
         */
        void flush(bool force);

        /**
         * @brief Get the value nextUpdateCheck (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         * 
         * This is the clock time when we should next stay online long enough for a software update check
         */
        time_t getValue_nextUpdateCheck() const {
            return (time_t) getValue_uint32(offsetof(SavedData, nextUpdateCheck));
        }

        void setValue_nextUpdateCheck(time_t value) {
            setValue_uint32(offsetof(SavedData, nextUpdateCheck), (uint32_t) value);
        }

        /**
         * @brief Get the value nextPublish (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        time_t getValue_nextPublish() const {
            return (time_t) getValue_uint32(offsetof(SavedData, nextPublish));
        }
        void setValue_nextPublish(time_t value) {
            setValue_uint32(offsetof(SavedData, nextPublish), (uint32_t)value);
        }

        /**
         * @brief Get the value nextQuickWake (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        time_t getValue_nextQuickWake() const {
            return (time_t) getValue_uint32(offsetof(SavedData, nextQuickWake));
        }
        void setValue_nextQuickWake(time_t value) {
            setValue_uint32(offsetof(SavedData, nextQuickWake), (uint32_t)value);
        }

        /**
         * @brief Gets a value from the savedData
         * 
         * @param offset Offset in bytes into savedData, typically offsetof(SavedData, fieldName)
         * @return uint32_t The value
         * 
         * This is a fast operation. It obtains a lock, but reads the value out of RAM.
         */
        uint32_t getValue_uint32(size_t offset) const;

        /**
         * @brief Sets a value in savedData
         * 
         * @param offset Offset in bytes into savedData, typically offsetof(SavedData, fieldName)
         * @param value The value to set
         * 
         * This method sets a value in the structure. Normally, if the value changed, then about
         * one second later the change will be saved to disk from the loop thread. The
         * savedData is also saved before sleep or reset if changed.
         * 
         * You can change the save delay by using withSaveDelayMs(). If you set it to 0, then
         * the data is saved within the setValue call immediately, which will make all set calls
         * run more slowly.
         */
        void setValue_uint32(size_t offset, uint32_t value);
        

        static const uint32_t SAVED_DATA_MAGIC = 0xd87cb6ce;
        static const uint16_t SAVED_DATA_VERSION = 1; 

    protected:
        /**
         * This class cannot be copied
         */
        PersistentData(const SettingsFile&) = delete;

        /**
         * This class cannot be copied
         */
        PersistentData& operator=(const SettingsFile&) = delete;

        SavedData savedData;

        uint32_t lastUpdate = 0;
        uint32_t saveDelayMs = 1000;

        String path;
    };
    
#ifndef UNITTEST
    SleepHelper &withSleepConfigurationFunction(std::function<bool(SystemSleepConfiguration &, std::chrono::milliseconds&)> fn) { 
        sleepConfigurationFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withWakeFunction(std::function<bool(const SystemSleepResult &)> fn) { 
        wakeFunctions.add(fn); 
        return *this;
    }
#endif

    SleepHelper &withSetupFunction(std::function<bool()> fn) { 
        setupFunctions.add(fn);
        return *this;
    }

    SleepHelper &withLoopFunction(std::function<bool()> fn) { 
        loopFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Determine if it's OK to sleep now, when in connected state
     * 
     * @param fn 
     * @return SleepHelper& 
     * 
     * The sleep ready function prototype is:
     * 
     * bool callback(system_tick_t connecteTimeMs)
     * 
     * Return true if your situation is OK to sleep now. This does not guaranteed that sleep will
     * actually occur, because there can be many sleep ready functions and other calculations.
     * 
     * Return false if you still have things to do before it's OK to sleep.
     * 
     * This callback is only called when connected. Scanning the list stops when the first
     * callback return false, so you should not use this callback for periodic actions. Use
     * withWhileConnectedFunction() instead.
     */
    SleepHelper &withSleepReadyFunction(std::function<bool(system_tick_t)> fn) {
        sleepReadyFunctions.add(fn); 
        return *this;
    }

    SleepHelper &withShouldConnectFunction(std::function<bool(int &connectConviction, int &noConnectConviction)> fn) { 
        shouldConnectFunctions.add(fn); 
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

    SleepHelper &withSettingChangeFunction(std::function<bool(const char *)> fn) { 
        settingsFile.withSettingChangeFunction(fn);
        return *this;
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


    SettingsFile settingsFile;
    PersistentData persistentData;

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

    AppCallback<SystemSleepConfiguration &, std::chrono::milliseconds&> sleepConfigurationFunctions;

    AppCallback<const SystemSleepResult &> wakeFunctions;

#endif // UNITTEST


    AppCallback<> setupFunctions;

    AppCallback<> loopFunctions;


    AppCallback<system_tick_t> sleepReadyFunctions;

    ShouldConnectAppCallback shouldConnectFunctions;


    AppCallback<> wakeOrBootFunctions;

    AppCallback<bool> sleepOrResetFunctions;

    AppCallback<system_tick_t> maximumTimeToConnectFunctions;

    AppCallback<> noConnectionFunctions;

    AppCallback<> whileConnectedFunctions;

    LocalTimeConvert::Schedule sleepSchedule;
    LocalTimeConvert::Schedule publishSchedule;

#ifndef UNITTEST
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
