#ifndef __SLEEPHELPER_H
#define __SLEEPHELPER_H

#include "Particle.h"
#include "LocalTimeRK.h"
#include "JsonParserGeneratorRK.h"
#include <vector>


/**
 *  @defgroup callbacks Callback functions you can register
 */

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
         * @brief Construct a SleepHelperRecursiveMutex wrapper object from an existing recursive mutex
         * 
         * @param handle The mutex handle
         * 
         * Ownership of the mutex handle is transferred to this object and it will be destroyed when this
         * object is deleted.
         */
        SleepHelperRecursiveMutex(os_mutex_recursive_t handle) : handle_(handle) {
        }

        /**
         * @brief Default constructor with no mutex - one will be created on first lock
         * 
         */
        SleepHelperRecursiveMutex() : handle_(nullptr) {
        }

        /**
         * @brief Destroys the underlying mutex object.
         * 
         */
        ~SleepHelperRecursiveMutex() {
            dispose();
        }

        /**
         * @brief Destroys the mutex object
         */
        void dispose() {
            if (handle_) {
                os_mutex_recursive_destroy(handle_);
                handle_ = nullptr;
            }
        }

        /**
         * @brief Locks the mutex. Creates a new recursive mutex object if it does not exist yet.
         * 
         * Blocks if another thread has obtained the mutex, continues when the other thread releases it.
         * 
         * Never call lock from a SINGLE_THREADED_BLOCK since deadlock can occur.
         */
        void lock() const { 
            if (!handle_) {
                os_mutex_recursive_create(&handle_);
            }
            os_mutex_recursive_lock(handle_); 
        }
        /**
         * @brief Attempts to lock the mutex. Creates a new recursive mutex object if it does not exist yet.
         * 
         * Returns true if the mutex was locked or false if another thread has already locked it.
         */
        bool trylock() const { 
            if (!handle_) {
                os_mutex_recursive_create(&handle_);
            }
            return os_mutex_recursive_trylock(handle_)==0; 
        }

        /**
         * @brief Attempts to lock the mutex. Creates a new recursive mutex object if it does not exist yet.
         * 
         * Returns true if the mutex was locked or false if another thread has already locked it.
         */
        bool try_lock() const { 
            return trylock(); 
        }

        /**
         * @brief Unlocks the mutex
         * 
         * If another thread is blocked on locking the mutex, that thread will resume.
         */
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
     * 
     * Callbacks can have different parameters, and this template allows the parameters to be specified.
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
         * 
         * The bool result is ignored when using forEach.
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
     * @brief State data for AppCallbackWithState
     * 
     * The state data includes an integer state number. Callbacks can also store
     * additional data in the callbackData field.
     * 
     * Since callbacks are typically never removed, you can put an allocated pointer
     * in the callbackData field.
     */
    class AppCallbackState {
    public:
        static const int CALLBACK_STATE_START = -1; //!< The callback has just started this wake cycle
        static const int CALLBACK_START_RETURNED_FALSE = -2; //!< The callback return false and should not be called again for this wake cycle

        int callbackState = CALLBACK_STATE_START; //!< The current state of this callback
        void *callbackData = 0; //!< Callback can store data here
    };

    /**
     * @brief Works like AppCallback, but includes additional state data
     * 
     * This is used by data capture functions.
     */
    template<class... Types>
    class AppCallbackWithState {
    public: 
        /**
         * @brief Adds a callback function. Zero or more callbacks can be defined.
         * 
         * @param callback 
         * 
         * The callback always returns a bool, but the parameters are defined by the template.
         */
        void add(std::function<bool(AppCallbackState &, Types... args)> callback) {
            callbackFunctions.push_back(callback);
            callbackState.push_back(AppCallbackState());
        }

        /**
         * @brief Set the state
         * 
         * @param newState The state number, such as CALLBACK_STATE_START
         * 
         * User code can use positive values for their own state
         */
        void setState(int newState) {
            for(auto it = callbackState.begin(); it != callbackState.end(); ++it) {
                it->callbackState = newState;
            }
        }

        /**
         * @brief Set the state to CALLBACK_STATE_START
         * 
         * This is done on each wake cycle.
         */
        void setStartState() {
            setState(AppCallbackState::CALLBACK_STATE_START);
        }

        /**
         * @brief Keeps calling the callback function while any callback returns true
         * 
         * @param args Parameters to pass to the callbacks
         * @return true Call this function again, as not all callbacks are done for this wake cycle.
         * @return false All callbacks are done
         * 
         * Once your callback returns false it will not be called again for this wake cycle.
         * Once all callbacks return false, this function returns false.
         */
        bool whileAnyTrue(Types... args) {
            bool finalRes = false;

            for(auto it = callbackState.begin(), it2 = callbackFunctions.begin(); it != callbackState.end(); ++it, ++it2) {
                if (it->callbackState != AppCallbackState::CALLBACK_START_RETURNED_FALSE) {

                    bool res = (*it2)(*it, args...);
                    if (res) {
                        finalRes = true;
                    }
                    else {
                        it->callbackState = AppCallbackState::CALLBACK_START_RETURNED_FALSE;
                    }
                }
            }
            return finalRes;
        }

        /**
         * @brief Returns true if there are no callbacks registered
         * 
         * @return true No callbacks registered
         * @return false At least one callback is registered
         */
        bool isEmpty() const { return callbackFunctions.empty(); };

        std::vector<std::function<bool(AppCallbackState &, Types... args)>> callbackFunctions; //!< The callback functions
        std::vector<AppCallbackState> callbackState; //!< The state for the callback functions. The array indexes match callbackFunctions.

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
        /**
         * @brief Determine if a cloud connection should occur
         * 
         * @return true Should connect to the cloud
         * @return false Should not connect to the cloud
         * 
         * This is determined by calling all of the shouldConnect functions. Each function is passed
         * a connectConviction and a noConnectConviction, both initialized to true.
         * 
         * If the callback believes a connection should be made, the connectConviction should be 
         * set to a value from 1 to 100. A typical value would be 60. This would encourage connection
         * but still could be overridden for other reasons, such as insufficient battery power.
         * 
         * If the callback believes a connection should not be made, it sets noConnectConviction to
         * a value from 1 to 100, 100 being definitely do not connect.
         * 
         * If the highest connection conviction is greater than the highest no connection 
         * conviction, then a connection will be attempted.
         * 
         * Callback prototype:
         * 
         * bool shouldConnectCallback(int &connectConviction, int &noConnectConviction);
         * 
         * Return true from the function in all cases.
         */
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

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
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
         * @brief Set value to the specified JSON, replacing the existing settings. Calls update callbacks if necessary.
         * 
         * @param json 
         * @return true 
         * @return false 
         */
        bool setValuesJson(const char *json);

        /**
         * @brief Merges multiple values from JSON data into the settings. Calls update callbacks if necessary.
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

        JsonParserStatic<particle::protocol::MAX_EVENT_DATA_LENGTH, 50> parser; //!< Parser for JSON data in settings file

        AppCallback<const char *> settingChangeFunctions; //!< Functions to call whenb settings change
        String path; //!< Path to the settings file
        const char *defaultValues = 0; //!< Default values for settings (not used with cloud-based settings)
    };
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Cloud-based settings
     * 
     * This is a specialized subclass of SettingsFile. It still stores JSON data in a file
     * on the flash file system. However, it is designed to be used with the cloud as the
     * source of truth. 
     * 
     * At specified times, the device will send up a 32-bit hash code of its current settings
     * in a publish. If the cloud has a different version of the settings, it will send
     * down a new configuration by Particle function call.
     * 
     * The new settings are designed to always fit in a single function payload, 1024 bytes
     * on most Gen 3 devices, and the entire configuration is always sent to make sure the
     * data and hash values match.
     * 
     * Because you cannot make local changes to settings, all of the set, update, and default
     * settings methods of SettingsFile are hidden when using CloudSettingsFile.
     */
    class CloudSettingsFile : public SettingsFile {
    public:
        CloudSettingsFile() {
        };

        /**
         * @brief Default values are never used with cloud settings, because the settings must originate from the cloud side
         * 
         * @param defaultValues 
         * @return SettingsFile& 
         */
        SettingsFile &withDefaultValues(const char *defaultValues) = delete;

        /**
         * @brief You can never set a value when using cloud settings because the source of truth is always the cloud version
         * 
         * @tparam T 
         * @param name 
         * @param value 
         * @return true 
         * @return false 
         */
        template<class T>
	    bool setValue(const char *name, const T &value) = delete;
        
        /**
         * @brief You can never set a value when using cloud settings because the source of truth is always the cloud version
         * 
         * @param name 
         * @param value 
         * @return true 
         * @return false 
         */
        bool setValue(const char *name, const char *value) = delete;

        /**
         * @brief Cloud settings must always exactly match the cloud version, so you only server setValueJson, not update
         * 
         * @param json 
         * @return true 
         * @return false 
         */
        bool updateValuesJson(const char *json) = delete;

        /**
         * @brief Default values are never used with cloud settings
         * 
         * @param inputJson 
         * @return true 
         * @return false 
         */
        bool addDefaultValues(const char *inputJson) = delete;

        /**
         * @brief Get the hashed value of the current settings, used to check if they need to be updated
         * 
         * @return uint32_t 
         */
        uint32_t getHash() const;

        /**
         * @brief The hash seed used for settings file changes
         */
        static const uint32_t HASH_SEED = 0x5b4ffa05;

        /**
         * @brief Murmur3 hash algorithm implementation
         * 
         * @param buf Pointer to the buffer to hash (const uint8_t *) 
         * @param len Length of the data in bytes
         * @param seed hash seed value (uint32_t)
         * @return uint32_t 
         * 
         * This implementation has been moved into the top-level SleepHelper
         * class and you should use that directly.
         */
        static uint32_t murmur3_32(const uint8_t* buf, size_t len, uint32_t seed) {
            return SleepHelper::murmur3_32(buf, len, seed);
        }

    private:

    };
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    /**
     * @brief Base class for storing persistent binary data to a file or retained memory
     * 
     * This class is separate from PersistentData so you can subclass it to hold your own application-specific
     * data as well.
     * 
     * See PersistentDataFile for saving data to a file on the flash file system.
     */
    class PersistentDataBase : public SleepHelperRecursiveMutex {
    public:
        /**
         * @brief Header at the beginning of all saved data stored in RAM, retained memory, or a file.
         */
        class SavedDataHeader { // 16 bytes
        public:
            uint32_t magic;                 //!< savedDataMagic, should rarely, if ever, change
            uint16_t version;               //!< savedDataVersion, should rarely, if ever, change
            uint16_t size;                  //!< size of the whole structure, including the user data after it
            uint32_t hash;                  //!< hash value for verifying data integrity
            uint32_t reserved1;             //!< reserved for future use
            // You cannot change the size of this structure without changing the version number!
        };
        
        /**
         * @brief Base class for persistent data saved in file or RAM
         * 
         * @param savedDataHeader Pointer to the header
         * @param savedDataSize size of the whole structure, including the user data after it 
         * @param savedDataMagic Magic bytes to use for this data
         * @param savedDataVersion Version to use for this data
         */
        PersistentDataBase(SavedDataHeader *savedDataHeader, size_t savedDataSize, uint32_t savedDataMagic, uint16_t savedDataVersion) : 
            savedDataHeader(savedDataHeader), savedDataSize(savedDataSize), savedDataMagic(savedDataMagic), savedDataVersion(savedDataVersion)  {
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
        virtual void setup();

        /**
         * @brief Load the persistent data file. You normally do not need to call this; it will be loaded automatically.
         * 
         * @return true 
         * @return false 
         */
        virtual bool load();

        /**
         * @brief Save the persistent data file. You normally do not need to call this; it will be saved automatically.
         * 
         * Save does nothing in this base class, but for PersistentDataFile it saves to a file
         */
        virtual void save() {};

        /**
         * @brief Write the settings to disk if changed and the wait to save time has expired
         * 
         * @param force Pass true to ignore the wait to save time and save immediately if necessary. This
         * is used when you're about to sleep or reset, for example.
         * 
         * This call is fast if a save is not required so you can call it frequently, even every loop.
         */
        virtual void flush(bool force);

       /**
         * @brief Either saves data or immediately, or defers until later, based on saveDelayMs
         * 
         * If saveDelayMs == 0, then always saves immediately. Otherwise, waits that amount of time before saving
         * to allow multiple saves to be batch and to not block the updating thread.
         */
        virtual void saveOrDefer();

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
         * @tparam T An integral types
         * @param offset Offset into the structure, normally offsetof(field, T)
         * @param value The value to set
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
                        savedDataHeader->hash = getHash();
                        saveOrDefer();
                    }
                }
            }
        }

        /**
         * @brief Get the value of a string
         * 
         * @param offset 
         * @param size 
         * @param value 
         * @return true 
         * @return false 
         * 
         * The templated getValue method doesn't work with String values, so this version should be used instead.
         */
        bool getValueString(size_t offset, size_t size, String &value) const;

        /**
         * @brief Set the value of a string
         * 
         * @param offset 
         * @param size 
         * @param value 
         * @return true 
         * @return false 
         * 
         * The templated setValue method doesn't work with string values, so this version should be used instead.
         */
        bool setValueString(size_t offset, size_t size, const char *value);
        
        
        /**
         * @brief Get the hash valid for data integrity checking
         */
        uint32_t getHash() const;

        static const uint32_t HASH_SEED = 0x851c2a3f;

    protected:
        /**
         * This class cannot be copied
         * 
         * Note that subclasses don't need to do include a definition like this because as long as the base class is 
         * copy-prevented subclasses by default cannot be copied.
         */
        PersistentDataBase(const PersistentDataBase&) = delete;

        /**
         * This class cannot be copied
         */
        PersistentDataBase& operator=(const PersistentDataBase&) = delete;

        /**
         * @brief Used to validate the saved data structure. Used internally by load(). 
         * 
         * @return true 
         * @return false
         * 
         * If you subclass this, be sure to call the superclass validate() first. If it returns false,
         * you should just return false and skip your own validation since the outer headers were not
         * valid and the structure cannot be used until reinitialized. 
         */
        virtual bool validate(size_t dataSize);

        /**
         * @brief Used to allow subclasses to initialize the saved data structure. Called internally by load(). 
         * 
         * If you subclass this, be sure to call the superclass initialize() after you update your fields short so
         * the hash value can be updated.
         */
        virtual void initialize();


        SavedDataHeader *savedDataHeader = 0; //!< Pointer to the saved data header, which is followed by the data
        uint32_t savedDataSize = 0;     //!< Size of the saved data (header + actual data)
        
        uint32_t savedDataMagic;        //!< Magic bytes for the saved data
        uint16_t savedDataVersion;      //!< Version number for the saved data

        uint32_t lastUpdate = 0; //!< Last time the file was updated. 0 = file has not changed since writing to disk.
        uint32_t saveDelayMs = 1000; //!< How long to wait to save before writing file to disk. Set to 0 to write immediately.
    };

#ifndef UNITTEST
    /**
     * @brief Base class for persistent data stored in emulated EEPROM
     * 
     * This is only recommended for Gen 2 devices. On Gen 3 devices, EEPROM is implemented as a file in the file
     * system anyway, so it's more efficient to just store your persistent data in its own file.
     * 
     */
    class PersistentDataEEPROM : public PersistentDataBase {
    public:
        /**
         * @brief Base class for persistent data saved in file
         * 
         * @param savedDataHeader Pointer to the saved data header
         * @param savedDataSize size of the whole structure, including the user data after it 
         * @param savedDataMagic Magic bytes to use for this data
         * @param savedDataVersion Version to use for this data
         */
        PersistentDataEEPROM(SavedDataHeader *savedDataHeader, size_t savedDataSize, uint32_t savedDataMagic, uint16_t savedDataVersion) : 
            PersistentDataBase(savedDataHeader, savedDataSize, savedDataMagic, savedDataVersion) {
        };
        
        /**
         * @brief Sets the offset into the EEPROM where the data will reside
         * 
         * @param eepromOffset
         * @return PersistentDataFile& 
         */
        PersistentDataEEPROM &withEepromOffset(int eepromOffset) { 
            this->eepromOffset = eepromOffset; 
            return *this; 
        };

        /**
         * @brief Load the persistent data file. You normally do not need to call this; it will be loaded automatically.
         * 
         * @return true 
         * @return false 
         */
        virtual bool load();

        /**
         * @brief Save the persistent data file. You normally do not need to call this; it will be saved automatically.
         */
        virtual void save();
    
    protected:
        int eepromOffset; //!< Offset into EEPROM to save the data
    };
    #endif // UNITTEST

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Base class for persistent data stored in a file 
     * 
     */
    class PersistentDataFile : public PersistentDataBase {
    public:
        /**
         * @brief Base class for persistent data saved in file
         * 
         * @param savedDataHeader Pointer to the saved data header
         * @param savedDataSize size of the whole structure, including the user data after it 
         * @param savedDataMagic Magic bytes to use for this data
         * @param savedDataVersion Version to use for this data
         */
        PersistentDataFile(SavedDataHeader *savedDataHeader, size_t savedDataSize, uint32_t savedDataMagic, uint16_t savedDataVersion) : 
            PersistentDataBase(savedDataHeader, savedDataSize, savedDataMagic, savedDataVersion) {
        };
        
        /**
         * @brief Sets the path to the persistent data file on the file system
         * 
         * @param path 
         * @return PersistentDataFile& 
         */
        PersistentDataFile &withPath(const char *path) { 
            this->path = path; 
            return *this; 
        };


        /**
         * @brief Load the persistent data file. You normally do not need to call this; it will be loaded automatically.
         * 
         * @return true 
         * @return false 
         */
        virtual bool load();

        /**
         * @brief Save the persistent data file. You normally do not need to call this; it will be saved automatically.
         */
        virtual void save();

    protected:
        String path; //!< Path to data file
    };
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Class for storing small data used by SleepHelper in the flash file system
     * 
     * You must not access the persistent data at global constructor time. Only use it from
     * setup() or later. You can access it from worker threads.
     * 
     * This class is only for SleepHelper private data. For storing your own data,
     * you should subclass PersistentDataBase (for retained memory) or 
     * subclass PersistentDataFile (for file-stored data).
     */
    class PersistentData : public PersistentDataFile {
    public:
        /**
         * @brief Structure saved to the persistent data file (binary)
         * 
         * It must always begin with the SavedDataHeader (16 bytes)!
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
            SavedDataHeader header; //!< Must be at the beginning of the data structure
            uint32_t lastUpdateCheck; //!< time_t last update check (Unix time, UTC)
            uint32_t lastFullWake; //!< time_t last full wake (Unix time, UTC)
            uint32_t lastQuickWake; //!< time_t last quick wake (Unix time, UTC)
            uint32_t nextDataCapture; //!< time_t next data capture time (Unix time, UTC)
            // OK to add more fields here later without incremeting version.
            // New fields will be zero-initialized.
        };

        /**
         * @brief Default constructor. Use withPath() to set the pathname if using this constructor
         */
        PersistentData() : PersistentDataFile(&sleepHelperData.header, sizeof(SleepHelperData), SAVED_DATA_MAGIC, SAVED_DATA_VERSION) {};

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

        /**
         * @brief Set lastUpdateCheck value (Unix time, UTC)
         * 
         * @param value time_t Unix time at UTC, like the value of Time.now()
         */
        void setValue_lastUpdateCheck(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, lastUpdateCheck), (uint32_t) value);
        }

        /**
         * @brief Get the value lastFullWake (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        time_t getValue_lastFullWake() const {
            return (time_t) getValue<uint32_t>(offsetof(SleepHelperData, lastFullWake));
        }
        /**
         * @brief Set lastFullWake value (Unix time, UTC)
         * 
         * @param value time_t Unix time at UTC, like the value of Time.now()
         */
        void setValue_lastFullWake(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, lastFullWake), (uint32_t)value);
        }

        /**
         * @brief Get the value lastQuickWake (Unix time, UTC)
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        time_t getValue_lastQuickWake() const {
            return (time_t) getValue<uint32_t>(offsetof(SleepHelperData, lastQuickWake));
        }

        /**
         * @brief Set lastQuickWake value (Unix time, UTC)
         * 
         * @param value time_t Unix time at UTC, like the value of Time.now()
         */
        void setValue_lastQuickWake(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, lastQuickWake), (uint32_t)value);
        }

        /**
         * @brief Get time of the next data capture
         * 
         * @return time_t Unix time at UTC, like the value of Time.now(). Can be 0 (no time set)
         * 
         * Note that this setting is the next time, not the last time, as several other settings
         * are. This improves efficiency and simplifies the code dramatically. On the minus side,
         * changing the schedule will only affect the next schedule, not the current schedule.
         * Since the data capture periods tend to be short, regular, and rarely changing this
         * probably won't be an issue.
         */
        time_t getValue_nextDataCapture() const {
            return (time_t) getValue<uint32_t>(offsetof(SleepHelperData, nextDataCapture));
        }

        /**
         * @brief Get time of the next data capture
         * 
         * @return time_t Unix time at UTC, like the value of Time.now()
         */
        void setValue_nextDataCapture(time_t value) {
            setValue<uint32_t>(offsetof(SleepHelperData, nextDataCapture), (uint32_t)value);
        }

    
        static const uint32_t SAVED_DATA_MAGIC = 0xd87cb6ce; //!< Magic bytes in the data structure
        static const uint16_t SAVED_DATA_VERSION = 1; //!< Version of the data structure


    protected:
        SleepHelperData sleepHelperData; //!< Data stored in the persistent data file
    };
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Class to manage small events, typically used for time-series data
     * 
     * For example, if you want to frequently measure the temperature, but publish the data 
     * less frequently, to save on cellular connections, battery, and data operations, this
     * class can be helpful.
     * 
     * Each event is JSON data. The data is stored in a single file in the flash file
     * system. When it's time to publish, all of the events that will fit in the appropriate
     * size will be aggregated into a single JSON array, reducing the number of data 
     * operations and speeding the Particle event publishing process, which is limited to
     * one event per second-ish.
     */
    class EventHistory : public SleepHelperRecursiveMutex {
    public:
        EventHistory() {};

        /**
         * @brief Sets the path of the event history file
         * 
         * @param path 
         * @return EventHistory& 
         */
        EventHistory &withPath(const char *path) {
            this->path = path;
            return *this;
        }

        /**
         * @brief Adds an event to the event history
         * 
         * @param jsonObj Must be a string containing a JSON object (including the surrounding {}).
         * 
         * If the data you want to send is an array, you should encapsulate the array under a key in the outermost
         * object. If the data is just a single primitive such as a number or string, also surround it with
         * a key and object.
         */
        void addEvent(const char *jsonObj);

        /**
         * @brief Adds an event to the event history using a callback and writer
         * 
         * @param callback The callback function, typically a lambda
         * @return EventCombiner& 
         * 
         * The callback/lambda has this prototype:
         * 
         * void callback(JSONWriter &writer)
         * 
         * You typically use it like:
         * 
         * SleepHelper::instance().addEvent([](JSONWriter &writer) {
		 *      writer.name("b").value(1111)
         *            .name("c").value("testing!");
		 * });
         * 
         * This is using the addEvent method of SleepHelper(), which just calls this
         * method on the correct object.
         */;
        void addEvent(std::function<void(JSONWriter &)>callback);

        /**
         * @brief Get saved events and insert them as an array to writer
         * 
         * @param writer 
         * @param maxSize 
         * @param removeEvents 
         * @return true 
         * @return false 
         * 
         * If there are no events (getHasEvent() == false), this method returns quickly
         * so there is no need to preflight this call with a test for having events.
         */
        bool getEvents(JSONWriter &writer, size_t maxSize, bool removeEvents = true);
        
        /**
         * @brief Remove the events last retrieved using getEvents
         * 
         * By default, events are removed automatically so there is no need to call
         * this. The function exists so you can do a two-phase removal. 
         * 
         * It's safe to add events in the time period between getEvents() and
         * removeEvents() however you should not have multiple getEvents() in a row
         * or from different threads, as you will get the same events multiple times
         * and data corruption can occur.
         * 
         * If the device resets between getEvents() and removeEvents(), the events
         * will be sent again later.
         */
        void removeEvents();

        /**
         * @brief Returns true if there are events to get using getEvents
         * 
         * @return true 
         * @return false 
         * 
         * This operation is fast, just checks a flag variable. It's not necessary to
         * check before calling getEvents(), because that method has the same check
         * internally.
         * 
         * Except for the first run after wake, when it needs to check the file
         * system to see if there is a file that was not processed before reboot.
         * This is why this method is not const.
         */
        bool getHasEvents();

    protected:
        /**
         * This class cannot be copied
         */
        EventHistory(const EventHistory&) = delete;

        /**
         * This class cannot be copied
         */
        EventHistory& operator=(const EventHistory&) = delete;

        String path; //!< path to the event history file
        bool firstRun = true; //!< Used to flag the first time the file has been accessed
        bool hasEvents = false; //!< True if there are events in the event history file
        size_t removeOffset = 0; //!< Where to remove events from
    };
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Class to handle building JSON events from multiple callbacks with priority 
     * and the ability to generate multiple events if necessary
     * 
     * The idea is that you want to combine all of the data into a single event if possible
     * to minimize data operations. Sometimes you have unimportant data that would be nice
     * to include if there's space but omit if not. And sometimes you have important 
     * data that must be sent, and it's OK to send multiple events if necessary.
     * 
     * This class also has a priority-based de-duplication of keys.
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
            String json; //!< JSON fragment, an object without the surrounding {}
            int priority = 0; //!< Priority 0 - 100 inclusive.
            std::vector<String> keys; //!< Top level keys
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
         * @brief Add a callback that is removed after generating events
         * 
         * @param fn 
         * @return EventCombiner& 
         */
        EventCombiner &withOneTimeCallback(std::function<bool(JSONWriter &, int &)> fn) { 
            oneTimeCallbacks.add(fn); 
            return *this;
        }

        /**
         * @brief Sets parameters for the EventHistory feature
         * 
         * @param path path to store the event history
         * @param key JSON key to publish event history items under
         * @return EventCombiner& 
         */
        EventCombiner &withEventHistory(const char *path, const char *key) {
            eventHistory.withPath(path);
            this->eventHistoryKey = key;
            return *this;
        }

        /**
         * @brief Adds an event to the event history (preformatted JSON)
         * 
         * @param jsonObj A string containing a complete JSON object surrounded by {}
         * 
         * See the version with a callback for an easier way to build the JSON.
         */
        EventCombiner &addEvent(const char *jsonObj) {
            eventHistory.addEvent(jsonObj);
            return *this;
        }

        /**
         * @brief Adds an event to the event history using a callback and writer
         * 
         * @param callback The callback function, typically a lambda
         * @return EventCombiner& 
         * 
         * The callback/lambda has this prototype:
         * 
         * void callback(JSONWriter &writer)
         * 
         * You typically use it like:
         * 
         * SleepHelper::instance().addEvent([](JSONWriter &writer) {
		 *      writer.name("b").value(1111)
         *            .name("c").value("testing!");
		 * });
         */
        EventCombiner &addEvent(std::function<void(JSONWriter &)>callback) {
            eventHistory.addEvent(callback);
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

        /**
         * @brief Clear the one-time callbacks
         * 
         * This is done automatically after generateEvents, but can be done manually in unusual cases.
         */
        void clearOneTimeCallbacks() {
            oneTimeCallbacks.removeAll();
        }

    protected:
        /**
         * This class cannot be copied
         */
        EventCombiner(const EventCombiner&) = delete;

        /**
         * This class cannot be copied
         */
        EventCombiner& operator=(const EventCombiner&) = delete;

        /**
         * @brief Used internally to generate events based on priority
         * 
         * @param callback The callback to call
         * @param buf The event buffer
         * @param maxSize The size of the event buffer
         * @param infoArray Data to be added to the event
         * 
         * A separate function is used because the process is run twice, once for the regular callbacks and once for the one-time callbacks.
         */
        void generateEventInternal(std::function<void(JSONWriter &, int &)> callback, char *buf, size_t maxSize, std::vector<EventInfo> &infoArray);

        AppCallback<JSONWriter &, int &> callbacks; //!< Callback functions
        AppCallback<JSONWriter &, int &> oneTimeCallbacks; //!< One-time use callback functions 
        EventHistory eventHistory; //!< Event history
        String eventHistoryKey; //!< Key to use when publishing the event history
    };
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    /**
     * @brief Class to hold data to be published by Particle.publish
     */
    class PublishData {
    public:
        /**
         * @brief Initialize empty object
         */
        PublishData() {};

        /**
         * @brief Initialize object with event name, no data payload, and default flags
         * 
         * @param eventName Event name (64 characters maximum)
         */
        PublishData(const char *eventName) : eventName(eventName) {}

        /**
         * @brief Initialize object with event name, event data, and default flags
         * 
         * @param eventName Event name (64 characters maximum)
         * 
         * @param eventData Event data (typically 1024 characters, but may be less in some cases)
         */
        PublishData(const char *eventName, const char *eventData) : eventName(eventName), eventData(eventData) {}

        /**
         * @brief Initialize object with event name, event data, and flags
         * 
         * @param eventName Event name (64 characters maximum)
         * 
         * @param eventData Event data (typically 1024 characters, but may be less in some cases)
         * 
         * @param flags Flags such as PRIVATE or NO_ACK
         */
        PublishData(const char *eventName, const char *eventData, PublishFlags flags) : eventName(eventName), eventData(eventData), flags(flags) {}

        String eventName; //!< Particle event name
        String eventData; //!< Particle event payload
        PublishFlags flags = PRIVATE; //!< Flags. Default is PRIVATE, can also use NO_ACK
    };

    /**
     * @brief Copies pre-formatted JSON into a writer
     * 
     * @param src Must be a valid JSON object or array as a string
     * @param writer 
     * 
     * This is used because JSONWriter does not have a method to write pre-formatted JSON into a
     * writer. It's a little inefficient but works. Note: The JSON that is inserted may not be
     * identical to the original. This is especially true for JSON that contains double
     * values because the decimal precision will likely change.
     */
    static void JSONCopy(const char *src, JSONWriter &writer);

    /**
     * @brief Copies pre-formatted JSON into a writer
     * 
     * @param src Must be a JSONValue containing an object, array, or primitive
     * @param writer 
     * 
     * This is used because JSONWriter does not have a method to write pre-formatted JSON into a
     * writer. It's a little inefficient but works. Note: The JSON that is inserted may not be
     * identical to the original. This is especially true for JSON that contains double
     * values because the decimal precision will likely change.
     */
    static void JSONCopy(const JSONValue &src, JSONWriter &writer);


#ifndef UNITTEST
    /**
     * @brief Structure of information about the next planned sleep
     * 
     * The sleep configuration function is passed this object and can modify the fields marked 
     * "override setting" to change the sleep behavior. The other fields are informational.
     */
    class SleepConfigurationParameters {
    public:
        // Informational fields to help you determine if you need to modify sleep behavior
        bool isConnected; //!< Currently connected to cellular if true
        system_tick_t timeUntilNextFullWakeMs; //!< Number of milliseconds until next full wake
        time_t nextFullWakeTime; //!< Time of next full wake (Unix time seconds since January 1, 1970, at UTC)
        uint64_t calculatedMillis; //!< System.millis() when the sleep duration was calculated

        // You can update these to change the sleep behavior
        system_tick_t sleepTimeMs; //!< Override setting for sleep duration
        bool disconnectCellular; //!< Override setting for disconnecting from cellular
    };


    /**
     * @brief Register a function to be called to configure sleep
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * The callback function has the prototype:
     * 
     * bool callback(SystemSleepConfiguration &sleepConfiguration, SleepConfigurationParameters &sleepParameters)
     * 
     * - sleepConfiguration is the object that is passed to `System.sleep()` you can refer to this or modify it, however
     * some parameters like duration should be set via the sleepParameters.
     * - sleepParameters include both informational fields (like whether you're currently connected to cellular)
     * as well as fields you can change to modify the sleep behavior.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withSleepConfigurationFunction(std::function<bool(SystemSleepConfiguration &, SleepConfigurationParameters&)> fn) { 
        sleepConfigurationFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Register a function to be called on wake from sleep
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * The wake function has the prototype:
     * 
     * bool callback(const SystemSleepResult &sleepResult);
     * 
     * - sleepResult is a const reference to the SystemSleepResult that was obtained right after wake.
     * 
     * You should return true in all cases from your callback.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withWakeFunction(std::function<bool(const SystemSleepResult &)> fn) { 
        wakeFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Sets the minimum time to disconnect from cellular. Default: 13 minutes.
     * 
     * @param timeMs 
     * @return SleepHelper& 
     * 
     * You can set this lower than 13 minutes, but beware of aggressive reconnection behavior.
     */
    SleepHelper &withMinimumCellularOffTime(std::chrono::milliseconds timeMs) { 
        minimumCellularOffTimeMs = timeMs.count();
        return *this;
    }

    /**
     * @brief Sets the minimum time to sleep. Default is 10 seconds.
     * 
     * @param timeMs 
     * @return SleepHelper& 
     * 
     * If the time to sleep is less than that, we stay awake until the event occurs.
     */
    SleepHelper &withMinimumSleepTimeMs(std::chrono::milliseconds timeMs) { 
        minimumSleepTimeMs = timeMs.count();
        return *this;
    }

#endif



    /**
     * @brief Adds a function to be called during setup()
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * You must register this callback before actually calling setup() for obvious
     * reasons. You will probably never need to use this, but it exists just in case.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withSetupFunction(std::function<bool()> fn) { 
        setupFunctions.add(fn);
        return *this;
    }

    /**
     * @brief Adds a function to be called on every call to loop()
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * You will normally register a more specific function, such as a data capture function, no connect function, etc.
     * but this function is provided just in case.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withLoopFunction(std::function<bool()> fn) { 
        loopFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief The data capture function is called on a schedule to capture data
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * The callback has this prototype
     * 
     * bool callback()
     * 
     * Return true if you need to be called back to finish capturing data
     * 
     * Return false if you are done capturing data.
     * 
     * This callback is called for quick wake, full wake, and while connected. It
     * runs in a parallel state machine to the connection state machine. 
     * 
     * @ingroup callbacks
     */
    SleepHelper &withDataCaptureFunction(std::function<bool(AppCallbackState &state)> fn) {
        dataCaptureFunctions.add(fn);
        return *this;
    }

    /**
     * @brief Determine if it's OK to sleep now, when in connected state
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * The sleep ready function prototype is:
     * 
     * bool callback(AppCallbackState &state, system_tick_t connecteTimeMs)
     * 
     * Return false if your situation is OK to sleep now. This does not guaranteed that sleep will
     * actually occur, because there can be many sleep ready functions and other calculations.
     * Once you return false your callback will no longe be called until the next wake cycle.
     * 
     * Return true if you still have things to do before it's OK to sleep. You callback will
     * continue to be called until it returns false.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withSleepReadyFunction(std::function<bool(AppCallbackState &, system_tick_t)> fn) {
        sleepReadyFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Function to call to determine if a full wake should be done
     * 
     * @param fn Callback function or C++11 lambda to call.
     * @return SleepHelper& 
     * 
     * The callback has the prototype:
     * 
     * bool callback(int &connectConviction, int &noConnectConviction)
     * 
     * If the callback believes a connection should be made, the connectConviction should be 
     * set to a value from 1 to 100. A typical value would be 60. This would encourage connection
     * but still could be overridden for other reasons, such as insufficient battery power.
     * 
     * If the callback believes a connection should not be made, it sets noConnectConviction to
     * a value from 1 to 100, 100 being definitely do not connect.
     * 
     * If the highest connection conviction is greater than the highest no connection 
     * conviction, then a connection will be attempted.
     * 
     * Your function should return true in all cases.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withShouldConnectFunction(std::function<bool(int &connectConviction, int &noConnectConviction)> fn) { 
        shouldConnectFunctions.add(fn); 
        return *this; 
    }

    /**
     * @brief Called during setup, after sleep, or an aborted sleep because duration was too short
     * 
     * @param fn 
     * @return SleepHelper& 
     * 
     * - WAKEUP_REASON_SETUP Called from setup after cold boot or reset
     * - WAKEUP_REASON_NO_SLEEP Called after sleep aborted as it was too short
     * - Any result code from for SystemSleepWakeupReason, such as:
     *   - SystemSleepWakeupReason::BY_GPIO
     *   - SystemSleepWakeupReason::BY_ADC
     *   - SystemSleepWakeupReason::BY_RTC
     *   - SystemSleepWakeupReason::BY_BLE
     *   - SystemSleepWakeupReason::BY_NETWORK
     * 
     * @ingroup callbacks
     */
    SleepHelper &withWakeOrBootFunction(std::function<bool(int)> fn) { 
        wakeOrBootFunctions.add(fn); 
        return *this;
    }

    
    /**
     * @brief Set the event name used for the wake event. Default: sleepHelper.
     * 
     * @param eventName 
     * @return SleepHelper& 
     */
    SleepHelper &withWakeEventName(const char *eventName) {
        wakeEventName = eventName;
        return *this;
    }

#if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
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
     * 
     * @ingroup callbacks
     */
    SleepHelper &withWakeEventFunction(std::function<bool(JSONWriter &, int &)> fn) {
        wakeEventFunctions.withCallback(fn);
        return *this;
    }
#endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)


#if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Add a callback to add to an event published on wake executed for a single wake publish
     * 
     * @param fn 
     * @return SleepHelper& 
     * 
     * @ingroup callbacks
     */
    SleepHelper &withWakeEventOneTimeFunction(std::function<bool(JSONWriter &, int &)> fn) {
        wakeEventFunctions.withOneTimeCallback(fn);
        return *this;
    }
#endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

#if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Simplified interface to create a one-time wake event with enable detection
     * 
     * @param flag 
     * @param fn 
     * @return SleepHelper& 
     * 
     * 
     */
    SleepHelper &withWakeEventFlagOneTimeFunction(uint64_t flag, std::function<void(JSONWriter &, int &)> fn) {
        if ((eventsEnabled & flag) != 0) {
            wakeEventFunctions.withOneTimeCallback([flag, fn](JSONWriter &writer, int &priority) {
                const char *name = eventsEnableName(flag);
                writer.name(name);
                priority = eventsEnablePriority(flag);
                fn(writer, priority);
                return true;
            });
        }
        return *this;
    }
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    #if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Sets parameters for the EventHistory feature
     * 
     * @param path path to store the event history
     * @param key JSON key to publish event history items under
     * @return EventCombiner& 
     */
    SleepHelper &withEventHistory(const char *path, const char *key) {
        wakeEventFunctions.withEventHistory(path, key);
        return *this;
    }

    /**
     * @brief Adds an event to the event history (preformatted JSON)
     * 
     * @param jsonObj A string containing a complete JSON object surrounded by {}
     * 
     * See the version with a callback for an easier way to build the JSON.
     */
    SleepHelper &addEvent(const char *jsonObj) {
        wakeEventFunctions.addEvent(jsonObj);
        return *this;
    }

    /**
     * @brief Adds an event to the event history using a callback and writer
     * 
     * @param callback The callback function, typically a lambda
     * @return EventCombiner& 
     * 
     * The callback/lambda has this prototype:
     * 
     * void callback(JSONWriter &writer)
     * 
     * You typically use it like:
     * 
     * SleepHelper::instance().addEvent([](JSONWriter &writer) {
     *      writer.name("b").value(1111)
     *            .name("c").value("testing!");
     * });
     */
    SleepHelper &addEvent(std::function<void(JSONWriter &)>callback) {
        wakeEventFunctions.addEvent(callback);
        return *this;
    }
    #endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

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
     * 
     * @ingroup callbacks
     */
    SleepHelper &withSleepOrResetFunction(std::function<bool(bool)> fn) { 
        sleepOrResetFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Adds a function to be called while connecting
     * 
     * @param fn 
     * @return SleepHelper& 
     * 
     * Function or lambda prototype:
     * 
     * bool callback(system_tick_t ms)
     * 
     * Return true to continue attempting to connect. Return false to stop trying to connect and go to sleep.
     * 
     * All maximum time to connect functions are called on every loop call during the connecting phase,
     * until one return false. This provides a way to be called periodically during connection.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withMaximumTimeToConnectFunction(std::function<bool(system_tick_t ms)> fn) {
        maximumTimeToConnectFunctions.add(fn); 
        return *this; 
    }

    /**
     * @brief Sets the maximum time to connect to the cloud. If this limit is exceeded, the device will go to sleep.
     * 
     * @param timeMs Time in milliseconds
     * @return SleepHelper& 
     * 
     * Ideally this should be at least 11 minutes. If you need to conserve power you can set it as low as 5 minutes or
     * so, however only do so if you use a sleep mode that will completely power down the modem. Otherwise, it's 
     * possible that then modem will never be fully reset.
     */
    SleepHelper &withMaximumTimeToConnect(system_tick_t timeMs) { 
        return withMaximumTimeToConnectFunction([timeMs](system_tick_t ms) {
            return (ms >= timeMs);
        }); 
    }

    /**
     * @brief Sets the maximum time to connect to the cloud. If this limit is exceeded, the device will go to sleep.
     * 
     * @param timeMs Time as a chrono literal, such as 5min for 5 minutes. 
     * @return SleepHelper& 
     * 
     * Ideally this should be at least 11 minutes. If you need to conserve power you can set it as low as 5 minutes or
     * so, however only do so if you use a sleep mode that will completely power down the modem. Otherwise, it's 
     * possible that then modem will never be fully reset.
     */
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
     * If you want to be called while connected, see the sleepReady function which 
     * does double duty as the whileConnected function.
     * 
     * @param fn 
     * @return SleepHelper& 
     * 
     * @ingroup callbacks
     */
    SleepHelper &withNoConnectionFunction(std::function<bool(AppCallbackState &state)> fn) {
        noConnectionFunctions.add(fn); 
        return *this;
    }

    /**
     * @brief Set the minimum amount of time to stay connected to the cloud (optional)
     * 
     * @param timeMs Time in milliseconds. There is also an overload that takes a chrono literal.
     * @return SleepHelper& 
     */
    SleepHelper &withMinimumConnectedTime(system_tick_t timeMs) { 
        return withSleepReadyFunction([timeMs](AppCallbackState &state, system_tick_t ms) {
            return (ms < timeMs);
        }); 
    }

    /**
     * @brief Set the minimum amount of time to stay connected to the cloud (optional)
     * 
     * @param timeMs Time as a chrono literal, such as 30s for 30 seconds. 
     * @return SleepHelper& 
     */
    SleepHelper &withMinimumConnectedTime(std::chrono::milliseconds timeMs) { 
        return withSleepReadyFunction([timeMs](AppCallbackState &state, system_tick_t ms) {
            return (ms < timeMs.count());
        }); 
    }

#if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    /**
     * @brief Function to call when settings change
     * 
     * @param fn The function or C++ lambda to call
     * @return SleepHelper& 
     * 
     * The setting change callback has the prototype:
     *
     * bool callback(const char *key)
     * 
     * - key is the settings key that was modified
     * 
     * You should return true from the function in all cases.
     * 
     * This is only called when the setting changes, not on every boot.
     * 
     * @ingroup callbacks
     */
    SleepHelper &withSettingChangeFunction(std::function<bool(const char *)> fn) { 
        settingsFile.withSettingChangeFunction(fn);
        return *this;
    }
#endif

    
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
        return *this;
    }
#endif


#if defined(__AB1805RK_H) || defined(DOXYGEN_DO_NOT_DOCUMENT)
    /**
     * @brief Stop and resume an AB1805 watchdog before sleep and reset
     * 
     * @param ab1805 A reference to the AB1805 object from the AB1805_RK library
     * @return SleepHelper& 
     * 
     * You must include AB1805_RK.h before SleepHelper.h to enable this method!
     */
    SleepHelper &withAB1805_WDT(AB1805 &ab1805) {
        withWakeFunction([&ab1805](const SystemSleepResult &) {
            // Resume watchdog after sleep
            ab1805.resumeWDT();
            return true;
        });
        withSleepOrResetFunction([&ab1805](bool) {
            ab1805.stopWDT();
            return true;
        });
        return *this;
    }
#endif

#if defined(__PUBLISHQUEUEPOSIXRK_H) || defined(DOXYGEN_DO_NOT_DOCUMENT)
    /**
     * @brief Connect the PublishQueuePosixRK library with this library
     * 
     * @param maxTimeToPublish The maximum time to allow PublishQueuePosixRK publishes before 
     * forcing sleep with unsent events. Default: send all queued events
     * @return SleepHelper& 
     * 
     * You must include PublishQueuePosixRK.h before SleepHelper.h to enable this method!
     */
    SleepHelper &withPublishQueuePosixRK(std::chrono::milliseconds maxTimeToPublish = 0ms) {
        withWakeOrBootFunction([](int) {
            // Default to paused queue on PublishQueuePosix
            PublishQueuePosix::instance().setPausePublishing(true);
            return true;
        });

        withSleepReadyFunction([maxTimeToPublish](AppCallbackState &state, system_tick_t ms) {
            bool canSleep = false;

            if (state.callbackState == AppCallbackState::CALLBACK_STATE_START) { 
                // On first call to sleep ready, resume publishing
                PublishQueuePosix::instance().setPausePublishing(false);
                state.callbackState = 1;
            }
            else {
                if (maxTimeToPublish.count() != 0 && ms >= maxTimeToPublish.count()) {
                    PublishQueuePosix::instance().setPausePublishing(true);
                }
                canSleep = PublishQueuePosix::instance().getCanSleep();
                if (canSleep) {
                    PublishQueuePosix::instance().setPausePublishing(true);
                    PublishQueuePosix::instance().writeQueueToFiles();
                }
            }

            // This callback returns false when you can sleep, and true to stay awake, so reverse boolean
            return !canSleep;
        });

        return *this;
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

    // When adding a constant here, be sure to update SleepHelperWakeEvents _wakeEvents[] as well!

    static const uint64_t eventsEnabledWakeReason           = 0x0000000000000001ul;  //!< "wr" wake reason (int) event
    static const uint64_t eventsEnabledTimeToConnect        = 0x0000000000000002ul;  //!< "ttc" time to connect event
    static const uint64_t eventsEnabledResetReason          = 0x0000000000000004ul;  //!< "rr" reset reason event
    static const uint64_t eventsEnabledBatterySoC           = 0x0000000000000008ul;  //!< "soc" report battery SoC on full wake

    /**
     * @brief Enable an eventsEnable flag. These determine whether the add values to the wake event
     * 
     * @param flag Flag bit value such as eventsEnabledWakeReason. Can pass multiple flags bits to this function.
     * @return SleepHelper& 
     */
    SleepHelper &withEventsEnabledEnable(uint64_t flag) {
        eventsEnabled |= flag;
        return *this;
    }
    
    /**
     * @brief Disable an eventsEnable flag. These determine whether the add values to the wake event
     * 
     * @param flag Flag bit value to clear, such as eventsEnabledWakeReason. Can pass multiple flags bits to this function.
     * @return SleepHelper& 
     */
    SleepHelper &withEventsEnabledDisable(uint64_t flag) {
        eventsEnabled &= ~flag;
        return *this;
    }
    

    /**
     * @brief Returns true if the eventsEnable flag is set
     * 
     * @param flag Flag bit value to test, such as eventsEnabledWakeReason
     * @return true 
     * @return false 
     */
    bool eventsEnableEnabled(uint64_t flag) const {
        return (eventsEnabled & flag) != 0;
    }

    /**
     * @brief Returns the priority value (0 - 100) for a given flag
     * 
     * @param flag Flag bit to get the priority for, such as eventsEnabledWakeReason
     * @return int Priority value. Returns 0 for an invalid flag bit.
     */
    static int eventsEnablePriority(uint64_t flag);

    /**
     * @brief Returns the event name (const c-string) for a given flag
     * 
     * @param flag Flag bit to get the priority for, such as eventsEnabledWakeReason
     * @return const char * event name, or empty string if an invalid flag name
     */
    static const char *eventsEnableName(uint64_t flag);

    // Logging enable flags
    static const uint64_t logEnabledNormal                  = 0x0000000000fffffful;  //!< Enable all normal message (default)
    static const uint64_t logEnabledVerbose                 = 0x000000ffff000000ul;  //!< Enable more verbose messages
    static const uint64_t logEnabledDebugging               = 0x00ffff0000000000ul;  //!< Enable debugging message
    static const uint64_t logEnabledAll                     = 0xfffffffffffffffful;  //!< Enable all messages

    // logEnabledNormal
    static const uint64_t logEnabledPublish                 = 0x0000000000000001ul;  //!< Log on each publish

    // logEnabledVerbose

    // logEnabledDebugging
    static const uint64_t logEnabledPublishData             = 0x0000010000000000ul;  //!< Log the full publish data
    static const uint64_t logEnabledHistoryData             = 0x0000020000000000ul;  //!< Log the full history data

    /**
     * @brief Enable logging for a specific type of log
     * 
     * @param flag The flag bit such as logEnabledPublish. Can specify multiple flags logically ORed together.
     * @return SleepHelper& 
     */
    SleepHelper &withLogEnabledEnable(uint64_t flag) {
        logEnabled |= flag;
        return *this;
    }
    
    /**
     * @brief Disable logging for a specific type of log
     * 
     * @param flag The flag bit such as logEnabledPublish. Can specify multiple flags logically ORed together.
     * @return SleepHelper& 
     */
    SleepHelper &withLogEnabledDisable(uint64_t flag) {
        logEnabled &= ~flag;
        return *this;
    }
    
    /**
     * @brief Returns true if the specified logging flag is set.
     * 
     * @param flag The flag bit such as logEnabledPublish. 
     * @return true Logging enabled for this flag
     * @return false Logging disabled for this flag
     */
    bool logEnableEnabled(uint64_t flag) const {
        return (logEnabled & flag) != 0;
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

#if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
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
#endif // HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)

    /**
     * @brief Class for managing publish and wake schedules
     * 
     */
    LocalTimeScheduleManager scheduleManager;

    /**
     * @brief Get the quick wake schedule
     * 
     * @return LocalTimeSchedule& 
     */
    LocalTimeSchedule &getScheduleQuick() {
        return scheduleManager.getScheduleByName("quick");
    }

    /**
     * @brief Get the full wake schedule
     * 
     * @return LocalTimeSchedule& 
     */
    LocalTimeSchedule &getScheduleFull() {
        return scheduleManager.getScheduleByName("full");
    }

    /**
     * @brief The data capture schedule determines when to call the data capture callback
     * 
     * @return LocalTimeSchedule& 
     * 
     */
    LocalTimeSchedule &getScheduleDataCapture() {
        return scheduleManager.getScheduleByName("data");
    }

    /**
     * @brief Murmur3 hash algorithm implementation
     * 
     * @param buf Pointer to the buffer to hash (const uint8_t *) 
     * @param len Length of the data in bytes
     * @param seed hash seed value (uint32_t)
     * @return uint32_t 
     * 
     * This generates a 32-bit hash of the specified buffer. This is a non-cryptographic hash
     * but the code is very small and fast. It's used for detecting settings changes.
     * 
     * See: https://en.wikipedia.org/wiki/MurmurHash
     */
    static uint32_t murmur3_32(const uint8_t* buf, size_t len, uint32_t seed);

    
    static const int WAKEUP_REASON_SETUP        = 0x10001; //!< Wakeup reason used on reset or cold boot, from setup()
    static const int WAKEUP_REASON_NO_SLEEP     = 0x10002; //!< Wakeup reason when we didn't actually sleep because the period was too short


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

    /**
     * @brief Part of the algorithm used by murmur3_32(). Used internally.
     * 
     * @param k 
     * @return uint32_t 
     */
    static inline uint32_t murmur_32_scramble(uint32_t k) {
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        return k;
    }

#ifndef UNITTEST
    /**
     * @brief A system event handler is so the code can be notified of things like firmware update started
     * 
     * @param event system event code (uint64_t)
     * @param param data depending on the event code
     */
    void systemEventHandler(system_event_t event, int param);

    /**
     * @brief A system event handler is so the code can be notified of things like firmware update started
     * 
     * @param event system event code (uint64_t)
     * @param param data depending on the event code
     * 
     * The Device OS API doesn't take a class member and instance so this static function is used instead.
     * Since the SleepHelper is a singleton, this is easy.
     */
    static void systemEventHandlerStatic(system_event_t event, int param);

    /**
     * @brief Calculate sleep settings 
     * 
     * @param isConnected True if currently connected to cellular
     * 
     * This uses the last and future cellular connection times and the sleep configuration callbacks. 
     * It's a separate function because it's called from both stateHandlerNoConnection and 
     * stateHandlerDisconnectBeforeSleep.
     */
    void calculateSleepSettings(bool isConnected);

    /**
     * @brief Calls the data capture handlers
     * 
     * This method is called continuously from loop(). This function determines if a 
     * capture is currently in progress (non-blocking) of if it's time to start a new
     * capture.
     */
    void dataCaptureHandler();

    /**
     * @brief Initial state at boot or after sleep
     * 
     * Next state: 
     * - stateHandlerNoConnection if a quick wake cycle
     * - stateHandlerConnectWait if a full wake cycle (cloud connection started before going into this state)
     */
    void stateHandlerStart();

    /**
     * @brief Waits for the Particle cloud connection to be made
     * 
     * Previous state:
     * - stateHandlerStart Cloud connections is started in stateHandlerStart.
     * 
     * Next state:
     * - stateHandlerTimeValidWait Connected to the cloud, waiting for a valid RTC 
     * - stateHandlerDisconnectBeforeSleep Took too long to connect to the cloud, go into sleep mode instead
     */
    void stateHandlerConnectWait();

    /**
     * @brief Wait for time synchronization from the cloud. This normally occurs very quickly (under 1 second).
     * 
     * Next state:
     * - stateHandlerConnectedStart
     */
    void stateHandlerTimeValidWait();

    /**
     * @brief Handles things after connecting to the cloud on a full wake
     * 
     * Steps include:
     * - Recording the full wake time in the persistent data file
     * - Storing the time to connect (ttc) wake event data (if enabled)
     * - Storing the battery SoC (soc) wake event data (if enabled)
     * 
     * Next state:
     * - stateHandlerConnectedWakeEvents
     */
    void stateHandlerConnectedStart();

    
    /**
     * @brief Prepares wake event data
     * 
     * - Waits for data capture to complete (should normally be done by now)
     * - Set the sleepReady state handler states to start
     * - Generate event payload from event history and wake events
     * 
     * Next state:
     * - stateHandlerConnected
     */
    void stateHandlerConnectedWakeEvents();

    /**
     * @brief Handles things while connected to the cloud
     * 
     * Attempts to publish all saved data. Once all data has been published, the sleep ready functions are 
     * called to see if all callbacks agree it's time to sleep.
     * 
     * Next state:
     * - stateHandlerPublishWait if a publish is in progress
     * - stateHandlerPublishRateLimit if a publish was successful and we need to wait before publishing more data 
     * - stateHandlerConnected stays in state if a publish failed
     * - stateHandlerDisconnectBeforeSleep all data has been published and sleep ready function indicate time to sleep
     * - stateHandlerReconnectWait if the cloud connection is lost
     */
    void stateHandlerConnected();

    /**
     * @brief Wait for a publish to complete
     * 
     * The state transition occurs from the background publish lambda implemented in stateHandlerConnected.
     *
     * Previous state:
     * - stateHandlerConnected
     * 
     * Next state:
     * - stateHandlerPublishRateLimit
     */
    void stateHandlerPublishWait();

    /**
     * @brief Waits so publishes only occur once per second
     * 
     * Next state: 
     * - stateHandlerConnected
     */
    void stateHandlerPublishRateLimit();

    /**
     * @brief If the cloud connection is lost, waits here
     * 
     * Next state:
     * - stateHandlerConnected Successfully reconnected to the cloud
     * - stateHandlerDisconnectBeforeSleep Timed out reconnecting to the cloud
     */
    void stateHandlerReconnectWait();

    /**
     * @brief Called on quick wake
     * 
     * Stays in this state until data capture is completed and all noConnectionFunctions return false.
     * 
     * Previous state:
     * - stateHandlerStart
     * 
     * Next state:
     * - stateHandlerSleep
     */
    void stateHandlerNoConnection();

    /**
     * @brief Possibly disconnect from cellular before sleep
     * 
     * Calls calculateSleepSettings() to calculate the sleep settings. A short wake cycle or
     * a sleep configuration function can override the settings and use cellular standby
     * mode instead.
     * 
     * If disconnecting, the Cellular.disconnect occurs while in this state.
     * 
     * Next state:
     * - stateHandlerSleep Sleep with cellular one
     * - stateHandlerDisconnectWait Wait for Cellular.disconnect to complete
     */
    void stateHandlerDisconnectBeforeSleep();

    /**
     * @brief Waits for Particle.disconnected() to be true
     * 
     * Once the cloud is disconnected, calls either Cellular.disconnect() or WiFi.disconnect().
     * 
     * Next state:
     * - stateHandlerWaitCellularDisconnected
     */
    void stateHandlerDisconnectWait();

    /**
     * @brief Waits for Cellular.ready or WiFi.ready to be false
     * 
     * Once disconnected, calls Cellular.off() or WiFi.off()
     * 
     * Next state:
     * - stateHandlerWaitCellularOff
     */
    void stateHandlerWaitCellularDisconnected();

    /**
     * @brief Waits until Cellular.isOff or WiFi.isOff to be true
     * 
     * Next state:
     * - stateHandlerSleep
     */
    void stateHandlerWaitCellularOff();

    /**
     * @brief Handles sleep
     * 
     * - Calls sleepOrResetFunctions 
     * - Adjust sleep time to account for the time to disconnect from the cloud and cellular (could be a couple seconds)
     * - Uses System.sleep to sleep
     * - Records the wakeup reason after wake
     * 
     * Next state: 
     * - stateHandlerSleepDone We've woken up
     * - stateHandlerSleepShort After adjusting sleep duration, the duration was too short so not sleeping at all
     */
    void stateHandlerSleep();

    /**
     * @brief Handles wake operations
     * 
     * - Calls wakeOrBootFunctions
     * - Records the wake reason (wr) in the wake event
     * 
     * Next state:
     * - stateHandlerStart
     */
    void stateHandlerSleepDone();
    
    /**
     * @brief Handles sleep periods less than minimumSleepTimeMs (default: 10 seconds) without actually sleeping
     * 
     * Next state:
     * - stateHandlerSleepDone
     */
    void stateHandlerSleepShort();

    AppCallback<SystemSleepConfiguration &, SleepConfigurationParameters&> sleepConfigurationFunctions; //!< Callback functions for adjusting sleep behavior

    AppCallback<const SystemSleepResult &> wakeFunctions; //!< Callback functions called on wake from sleep

    SystemSleepConfiguration sleepConfig; //!< Passed to sleep configuration functions
    SleepConfigurationParameters sleepParams;  //!< Passed to sleep configuration functions

#endif // UNITTEST


    AppCallback<> setupFunctions; //!< Callback functions called during setup()

    AppCallback<> loopFunctions; //!< Callback functions called during loop()

    AppCallbackWithState<> dataCaptureFunctions; //!< Callback functions called for data capture


    AppCallbackWithState<system_tick_t> sleepReadyFunctions; //!< Callback functions to determine if it's time to sleep


    ShouldConnectAppCallback shouldConnectFunctions; //!< Callback functions to determine whether to do a quick or full wake


    AppCallback<int> wakeOrBootFunctions; //!< Called at either boot or after wake

    AppCallback<bool> sleepOrResetFunctions; //!< Called right before sleep or before reset

    AppCallback<system_tick_t> maximumTimeToConnectFunctions; //!< Callback to determine if the maximum time to connect has been exceeded

    /**
     * @brief 
     * 
     * When in no connection (quick wake) mode, after the data capture functions are called the 
     * no connection functions are called. 
     */
    AppCallbackWithState<> noConnectionFunctions;

    String wakeEventName = "sleepHelper"; //!< Event name for wake events. Default: "sleepHelper"

#if HAL_PLATFORM_FILESYSTEM || defined(UNITTEST)
    EventCombiner wakeEventFunctions; //!< Handlers to create wake events
#endif
    int wakeReasonInt = 0; //!< Wake reason after sleep

    std::vector<PublishData> publishData; //!< Wake event data to publish (JSON strings)

    /**
     * @brief Which event history events are enabled (default: all)
     * 
     * See constants such as eventsEnabledWakeReason, eventsEnabledTimeToConnect for flag values
     */
    uint64_t eventsEnabled = 0xfffffffffffffffful;

    /**
     * @brief Which logging messages to enable
     * 
     * Default: logEnabledNormal
     */
    uint64_t logEnabled = logEnabledNormal;

#ifndef UNITTEST
    system_tick_t minimumCellularOffTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(13min).count(); //!< Default value for the minimum time to turn cellular off
    system_tick_t minimumSleepTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(10s).count(); //!< Default value for the minimum time to sleep

    std::function<void(SleepHelper&)> stateHandler = &SleepHelper::stateHandlerStart; //!< state handler function
    system_tick_t stateTime = 0; //!< millis counter used in certain state handlers

    system_tick_t connectAttemptStartMillis = 0; //!< millis value when Particle.connect was called
    system_tick_t reconnectAttemptStartMillis = 0; //!< millis value when Particle.connected returned false after being connected
    system_tick_t networkConnectedMillis = 0; //!< mills value when Cellular.connected returned true
    system_tick_t connectedStartMillis = 0; //!< millis value when Particle.connected returned true
    system_tick_t lastEventHistoryCheckMillis = 0; //!< millis value the last time the event history was checked

    bool outOfMemory = false; //!< Set to true if an out of memory system event occurs
    
    /**
     * @brief True if data capture handlers are being called
     * 
     * This goes true right before they are called and will remain true until the last
     * one returns false. At this point, sleep can occur.
     */
    bool dataCaptureActive = false;

    /**
     * @brief Wake events are stored in this vector to be published, rate limited, later
     */
    std::vector<String> wakeEventPayload;

    /**
     * @brief Used instead of Cellular.ready(), etc.
     */     
#if Wiring_Cellular
    NetworkClass &network = Cellular;
#endif
#if Wiring_WiFi
    NetworkClass &network = WiFi;
#endif

#endif // UNITTEST

    /**
     * @brief Logger instance used by SleepHelper
     * 
     * All logging messages use the category app.sleep so you can control the level in 
     * your log handler instances. 
     * 
     * Within this library, always use appLog.info() instead of Log.info(), for example.
     */
    Logger appLog;


    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static SleepHelper *_instance;

};
#endif  /* __SLEEPHELPER_H */
