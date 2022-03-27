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

#ifndef UNITTEST
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
#endif /* UNITTEST */

    /**
     * @brief Base class for a list of zero or more callback functions
     * 
     * @tparam Types 
     */
    template<class... Types>
    class AppCallback {
    public:
        /**
         * @brief Adds a callback function. Zero or more callbacks can be defined.
         * 
         * @param callback 
         * 
         * The callback always returns a bool, but the parameters are defined by the template.
         */
        void add(std::function<bool(Types... args)> callback) {
            callbackFunctions.push_back(callback);
        }

        /**
         * @brief Calls all callbacks, regardless of return value returned.
         * 
         * @param args 
         */
        void forEach(Types... args) {
            for(auto it = callbackFunctions.begin(); it != callbackFunctions.end(); ++it) {
                (*it)(args...);
            }
        }

        /**
         * @brief Calls callbacks until the first one returns true. The others are not called.
         * 
         * @param defaultResult The value to return if all callbacks return false.
         * @param args 
         * @return true If any callback returned true
         * @return false If all callbacks returned false
         * 
         * This is fast return true, see also whileAnyTrue.
         */
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

        /**
         * @brief Calls all callbacks. Returns true if any returns true, but all are still called.
         * 
         * @param defaultResult The value to return if all callbacks return false.
         * @param args 
         * @return true 
         * @return false 
         */
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

        /**
         * @brief Calls all callbacks until the first one returns false, then returns without calling others.
         * 
         * @param defaultResult The value to return if all callbacks return true.
         * @param args 
         * @return true 
         * @return false 
         * 
         * This is fast return false. See also whileAnyFalse.
         */
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

        /**
         * @brief Calls all callbacks. If any returns false then returns false, but all are still called.
         * 
         * @param defaultResult The value to return if all callbacks return true.
         * @param args 
         * @return true 
         * @return false 
         */
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
         * to remove a single callback since they're typically lambdas and it would be difficult to
         * specify which one to remove.
         */
        void removeAll() {
            callbackFunctions.clear();
        }

        /**
         * @brief Vector of all callbacks, limited only by available RAM.
         */
        std::vector<std::function<bool(Types... args)>> callbackFunctions;
    };

    /**
     * @brief Class for ShouldConnect application callback
     * 
     * The prototype for the function or lambda is:
     * 
     * bool callback(int &connectConviction, int &noConnectConviction)
     * 
     * If you believe you should connect set connectConviction to a value between 1 and 100. The
     * value defaults 0 which means "I don't care". If you absolutely must connect to the cloud 
     * now, set the value to a high value.
     * 
     * If you do not want to connect, set the noConnectConviction to a value between 1 and 100.
     * For example, if you definitely do not have enough battery power to successfully connect
     * a high value is used.
     * 
     * All of the ShouldConnect callbacks are called, and the maximum values for connectConviction
     * and noConnectConviction are saved. If connectConviction >= noConnectConviction then
     * a connection is attempted.
     * 
     * The bool result is ignored by this callback.
     */
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
         * @brief Default values 
         * 
         * @param defaultValues 
         * @return SettingsFile& 
         */
        SettingsFile &withDefaultValues(const char *defaultValues) {
            this->defaultValues = defaultValues;
            return *this;
        }

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
                result = parser.getOuterValueByKey(name, value);
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
         * This is a merge; values that are not included in json but already exist in the settings
         * will be left unchanged.
         */
        bool updateValuesJson(const char *json);

        /**
         * @brief Merge in default values
         * 
         * @param inputJson 
         * @return true 
         * @return false 
         * 
         * This is like updateValuesJson but only updates the values from inputJson if the 
         * value does not already exist in the settings. This allows the initial set of default
         * settings to be created. It's also used on every load, so if you add a new default
         * setting in defaultValues, then that value will be added to the settings.
         */
        bool addDefaultValues(const char *inputJson);

        /**
         * @brief Get all of the current settings as a JSON string
         * 
         * @param json 
         * @return true 
         * @return false 
         * 
         * If you are getting a single value you should use getValue instead. This method is used
         * so the cloud can get all settings from a calculated variable.
         */
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
        const char *defaultValues = 0;
    };


    /**
     * @brief Base class for storing persistent binary data to a file
     * 
     * This class is separate from PersistentData so you can subclass it to hold your own application-specific
     * data as well.
     */
    class PersistentDataBase : public SleepHelperRecursiveMutex {
    public:
        class SavedDataHeader { // 20 bytes
        public:
            uint32_t magic;                 //!< SAVED_DATA_MAGIC = 0xd87cb6ce;
            uint32_t version;               //!< SAVED_DATA_VERSION = 1
            uint16_t size;                  //!< size of the whole structure, including the user data after it
            uint16_t flags;                 //!< currently 0
            uint32_t reserved2;             //!< reserved for future use
            uint32_t reserved1;             //!< reserved for future use
            // You cannot change the size of this structure!
        };

        PersistentDataBase(SavedDataHeader *savedDataHeader, size_t savedDataSize) : savedDataHeader(savedDataHeader), savedDataSize(savedDataSize) {
        };

        PersistentDataBase(SavedDataHeader *savedDataHeader, size_t savedDataSize, const char *path) : savedDataHeader(savedDataHeader), savedDataSize(savedDataSize), path(path) {
        };


        /**
         * @brief Sets the path to the persistent data file on the file system
         * 
         * @param path 
         * @return PersistentData& 
         */
        PersistentDataBase &withPath(const char *path) { 
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
         * sleepHelperData is also saved before sleep or reset if changed.
         * 
         * You can change the save delay by using withSaveDelayMs(). If you set it to 0, then
         * the data is saved within the setValue call immediately, which will make all set calls
         * run more slowly.
         */
        PersistentDataBase &withSaveDelayMs(uint32_t value) {
            saveDelayMs = value;
            if (saveDelayMs == 0) {
                flush(true);
            }
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
         * @brief Templated class for getting integral values (uint32_t, float, double, etc.)
         * 
         * @tparam T 
         * @param offset 
         * @return T 
         */
        template<class T>
        T getValue(size_t offset) const {
            T result = 0;

            WITH_LOCK(*this) {
                if (offset <= (savedDataSize - sizeof(T))) {
                    const uint8_t *p = (uint8_t *)savedDataHeader;
                    p += offset;
                    result = *(const T *)p;
                }
            }
            return result;
        }

        /**
         * @brief Templated class for setting integral values (uint32_t, float, double, etc.)
         * 
         * @tparam T 
         * @param offset 
         * @return T 
         */
        template<class T>
        void setValue(size_t offset, T value)  {
            WITH_LOCK(*this) {
                if (offset <= (savedDataSize - sizeof(T))) {
                    uint8_t *p = (uint8_t *)savedDataHeader;
                    p += offset;

                    T oldValue = *(T *)p;
                    if (oldValue != value) {
                        *(T *)p = value;
                        if (saveDelayMs) {
                            lastUpdate = millis();
                        }
                        else {
                            save();
                        }
                    }
                }
            }
        }

        bool getValueString(size_t offset, size_t size, String &value) const;

        bool setValueString(size_t offset, size_t size, const char *value);
        

        static const uint32_t SAVED_DATA_MAGIC = 0xd87cb6ce;
        static const uint16_t SAVED_DATA_VERSION = 1; 

    protected:
        /**
         * This class cannot be copied
         */
        PersistentDataBase(const PersistentDataBase&) = delete;

        /**
         * This class cannot be copied
         */
        PersistentDataBase& operator=(const PersistentDataBase&) = delete;

        SavedDataHeader *savedDataHeader = 0;
        uint32_t savedDataSize = 0;
        
        uint32_t lastUpdate = 0;
        uint32_t saveDelayMs = 1000;

        String path;
    };

    /**
     * @brief Class for managing small persistent data
     * 
     * You must not access the persistent at global constructor time. Only use it from
     * setup() or later. You can access it from worker threads.
     * 
     */
    class PersistentData : public PersistentDataBase {
    public:
        /**
         * @brief Structure saved to the persistent data file (binary)
         * 
         * It must always begin with the SavedDataHeader (20 bytes)!
         * 
         * You can expand the structure later without incrementing the
         * version number. Added fields will be initialized to 0. The size is 
         * limited to 65536 bytes (uint16_t maximum value).
         * 
         * Since the SleepHelperData structure is always stored in RAM, you should not
         * make it excessively large.
         */
        class SleepHelperData {
        public:
            SavedDataHeader header;
            uint32_t lastUpdateCheck;
            uint32_t lastPublish;
            uint32_t lastQuickWake;
            // OK to add more fields here later without incremeting version.
            // New fields will be zero-initialized.
        };

        /**
         * @brief Default constructor. Use withPath() to set the pathname if using this constructor
         */
        PersistentData() : PersistentDataBase(&sleepHelperData.header, sizeof(SleepHelperData)) {};

        /**
         * @brief Constructor that thats a pathname to the persistent data file
         * 
         * @param path 
         */
        PersistentData(const char *path) : PersistentDataBase(&sleepHelperData.header, sizeof(SleepHelperData), path) {};

        /**
         * @brief Destructor
         */
        virtual ~PersistentData() {};


        /**
         * @brief Get the value lastUpdateCheck (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         * 
         * This is the clock time when we should next stay online long enough for a software update check
         */
        time_t getValue_lastUpdateCheck() const {
            return (time_t) getValue<uint32_t>(offsetof(SleepHelperData, lastUpdateCheck));
        }

        void setValue_lastUpdateCheck(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, lastUpdateCheck), (uint32_t) value);
        }

        /**
         * @brief Get the value lastPublish (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        time_t getValue_lastPublish() const {
            return (time_t) getValue<uint32_t>(offsetof(SleepHelperData, lastPublish));
        }
        void setValue_lastPublish(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, lastPublish), (uint32_t)value);
        }

        /**
         * @brief Get the value lastQuickWake (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        time_t getValue_lastQuickWake() const {
            return (time_t) getValue<uint32_t>(offsetof(SleepHelperData, lastQuickWake));
        }
        void setValue_lastQuickWake(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, lastQuickWake), (uint32_t)value);
        }

    protected:
        SleepHelperData sleepHelperData;
    };

    /**
     * @brief Class to handle building JSON events from multiple callbacks with priority 
     * and the ability to generate multiple events if necessary
     * 
     * The idea is that you want to combine all of the data into a single event if possible
     * to minimize data operations. Sometimes you have unimportant data that would be nice
     * to include if there's space but omit if not. And sometimes you have important 
     * data that must be sent, and it's OK to send multiple events if necessary.
     */
    class EventCombiner {
    public:
        /**
         * @brief Container to hold a JSON fragment and a priority value 0 - 100.
         * 
         * Note that this is only a fragment, basically an object without the surrounding {}!
         */
        class EventInfo {
        public:
            /**
             * @brief Default constructor 
             */
            EventInfo() {};

            /**
             * @brief Constructor with values to fill int
             * 
             * @param jsonStr The fragment of JSON to save 
             * @param priority The priority 0 - 100
             */
            EventInfo(const char *jsonStr, int priority) : json(jsonStr), priority(priority) {};

            String json; //!< JSON fragment, an object without the surrounding {}
            int priority = 0; //!< Priority 0 - 100 inclusive.
        };

        /**
         * @brief Default constructor
         * 
         * Use withCallback to add callback functions
         */
        EventCombiner() {};

        /**
         * @brief Adds a callback function to generate JSON data
         * 
         * @param fn 
         * @return EventCombiner& 
         * 
         * The callback function has this prototype:
         * 
         * bool callback(JSONWriter &writer, int &priority)
         * 
         * The return value is ignored; you should return true.
         * 
         * writer is the JSONWriter to store the data into
         * priority should be set to a value from 1 to 100. If you leave it at zero your data will not be saved!
         * 
         * Items are added to the event in priority order, largest first.
         * 
         * If you have a priority < 50 and the event is full, then your data will be discarded to 
         * avoid generating another event.
         */
        EventCombiner &withCallback(std::function<bool(JSONWriter &, int &)> fn) { 
            callbacks.add(fn); 
            return *this;
        }

        /**
         * @brief Generate one or more events based on the maximum event size
         * 
         * @param events vector of String objects to fill in with event data
         * 
         * The events vector you pass into this method will be cleared. It will be returned filled in
         * with zero or more Strings, each containing event data in valid JSON format.
         */
        void generateEvents(std::vector<String> &events);

        /**
         * @brief generate one or more events based on desired size
         * 
         * @param events vector of String objects to fill in with event data
         * @param maxSize Maximum size of each even in bytes
         * 
         * The events vector you pass into this method will be cleared. It will be returned filled in
         * with zero or more Strings, each containing event data in valid JSON format.
         */
        void generateEvents(std::vector<String> &events, size_t maxSize);


    protected:
        /**
         * This class cannot be copied
         */
        EventCombiner(const EventCombiner&) = delete;

        /**
         * This class cannot be copied
         */
        EventCombiner& operator=(const EventCombiner&) = delete;

        AppCallback<JSONWriter &, int &> callbacks; //!< Callback functions

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
     * @brief Add a callback to add to an event published on wake
     * 
     * @param fn Callback function
     * @return SleepHelper& 
     * 
     * This is used to efficiently publish data on each full wake with cloud connect. You can register
     * a callback to add data to a JSON object. There is a priority ordering and the ability to split
     * JSON data across multiple publishes if it does not fit, or discard unimportant data. This allows
     * multiple parts of your code to efficiently combine data into a single publish without having
     * to worry about size limits.
     * 
     * Wake event functions are only called when a full cloud connect is already planned, you can
     * influence this using a shouldConnect callback.
     * 
     * The callback function has this prototype:
     * 
     * bool callback(JSONWriter &writer, int &priority)
     * 
     * The return value is ignored; you should return true.
     * 
     * writer is the JSONWriter to store the data into
     * priority should be set to a value from 1 to 100. If you leave it at zero your data will not be saved!
     * 
     * Items are added to the event in priority order, largest first.
     * 
     * If you have a priority < 50 and the event is full, then your data will be discarded to 
     * avoid generating another event.
     */
    SleepHelper &withWakeEventFunction(std::function<bool(JSONWriter &, int &)> fn) {
        wakeEventFunctions.withCallback(fn);
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

    /**
     * @brief Class for managing the settings file
     * 
     * The settings file is stored as a file in the flash file system and is a JSON object
     * that is flat (one level deep, no sub-objects or arrays). 
     */
    SettingsFile settingsFile;

    /**
     * @brief Class for managing persistent data
     * 
     * Persistent data is stored as a file in the flash file system,
     */
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

    EventCombiner wakeEventFunctions;

    LocalTimeSchedule sleepSchedule;
    LocalTimeSchedule publishSchedule;

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
