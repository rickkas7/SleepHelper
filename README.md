# SleepHelper

*Library for simplifying Particle applications that use sleep modes*

Note that this library is currently incomplete and may still contain bugs. I'm releasing it now to get initial feedback and feature suggestions.

Especially important are:
- Any places where you think you'll need additional callback functions to customize behavior so that you can use the library without having to modify it
- Additional built-in wake events (optional)
- Additional logging (optional)
- Additional features needed for other use cases

Repository details:

- [Full browsable HTML documentation](https://rickkas7.github.io/SleepHelper/index.html).
- License: MIT
- Repository: https://github.com/rickkas7/SleepHelper/

## Example

This is the simplest example, from the "01-simple" example directory:

```cpp
#include "SleepHelper.h"

SerialLogHandler logHandler;

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);


void setup() {
    SleepHelper::instance().withShouldConnectMinimumSoC(9.0);

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();
}
```

Things to note in this code:


```cpp
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
```

You must enable the system thread and use SEMI_AUTOMATIC system mode. The library will not work properly in other modes.


```cpp
SleepHelper::instance().withShouldConnectMinimumSoC(9.0);
```

This pattern is used to set configuration parameters. The SleepHelper class is a singleton so you always use `SleepHelper::instance()` to access it. This method is fast and you should just call it every time instead of saving the value in a variable. You then add additional statements for what you want to configure. This this case, it will only connect to cellular if the battery SoC (state of charge) is greater than 9.0 %. 


```cpp
SleepHelper::instance().setup();
```
Once you've set all parameters, call the setup() method:


```cpp
SleepHelper::instance().loop();
```
In loop(), always call the loop method:

There are descriptions of additional examples at the end of this page.

## Design

The library is intended to be completely configurable and extendable without modifying the core library itself. 

### Adding functions

For example:

```cpp
SleepHelper::instance()
    .withSleepConfigurationFunction([](SystemSleepConfiguration &sleepConfig, SleepHelper::SleepConfigurationParameters &params) {
        // Add a GPIO wake on button press
        sleepConfig.gpio(BUTTON_PIN, FALLING);
        return true;
    })
```

This example adds a sleep configuration function. It allows you to execute code to customize sleep behavior before going to sleep. This uses C++11 lambdas to allow the code to be written inline, right inside the setup() function. You can use a standard function call, instead. Just remember that the body of the lambda is executed later, right before sleep, not during setup.

In this specific example, a GPIO wake from a button is added to allow wake from button press.

```cpp
SleepHelper::instance()
    .withWakeFunction([](const SystemSleepResult &sleepResult) {
        if (sleepResult.wakeupReason() == SystemSleepWakeupReason::BY_GPIO) {
            pin_t whichPin = sleepResult.wakeupPin();
            Log.info("wake by pin %d", whichPin);
            if (whichPin == BUTTON_PIN) {
                logButtonPress();
                lastButtonPress = pinWakeMillis = millis();
            }
            else {
                pinWakeMillis = 0;
            }
        }
        return true;
    })
```

This adds a wake function, which is called immediately after wake. This is done so we can keep track of the reason we woke (button or time).

### Quick wake vs. full wake

The library supports two different levels of wake:

- A quick wake or non-connected wake will wake up the device and go back to sleep very quickly, without connecting to cellular
- A full wake or connected wake will wake up and connect to the cloud

A quick wake might be used to read a sensor, save the data to flash, and go back to sleep. Or for a button press, vehicle sensor, etc. it might count the event, then go back to sleep. Often this can be done in under a second.

A full wake allows the data to be uploaded to the cloud, but takes longer and uses more power.

It's also possible to use the library with cellular standby mode, and also with Wi-Fi devices, but the concepts are similar.

### Management of modem power

For most applications that sleep longer than 15 minutes, cellular off mode is the recommended mode. 

For sleep cycles less than 10 minutes, however, you run the risk of getting your SIM banned from the cellular network for aggressive reconnection if you completely power it off.

The library provides the option of automatically optimizing the modem power-down behavior so short sleep cycles keep the modem on, to avoid aggressive reconnection. This can also save time and sometimes power because reconnecting to cellular may use more power than is saved by shutting down the modem for short sleep cycles, especially on 2G/3G devices.

### Wake events

The library includes an "EventCombiner" feature that allows your code to register a function (or lambda) to add JSON data to an event that is published at wake. You also set the priority of your data (1 - 100).

This allows multiple independent parts of the code to add data to a single JSON event to minimize data operations. If there is sufficient high priority data, multiple events are automatically created, but if possible the data is combined. 

If the event is full and there is low priority data (less than priority 50), the lowest priority is discarded first to allow the data to fit in a single event instead of creating multiple events.

There are also a number of built-in wake events, each of which can be turned off if you don't want the information. For example:

```json
{"soc":8.8,"ttc":16186,"wr":4}
```

- soc is the battery state of charge (0-100%)
- ttc is the time to connect to the cloud in milliseconds
- wr is the wake reason code (4 = by time)

### Data capture

```cpp
SleepHelper::instance()
    .withDataCaptureFunction([](SleepHelper::AppCallbackState &state) {
        if (Time.isValid()) {
            SleepHelper::instance().addEvent([](JSONWriter &writer) {
                writer.name("t").value((int) Time.now());
                writer.name("c").value(readTempC(), 1);
            });
        }
        return false;
    })
```

In other words, adding a wake event does not equal a single new publish. Wake events are a fragment of JSON that is added to other fragments of JSON to produce one or more JSON events. This reduces the number of data operations while also freeing you from having to worry about adding so many fragments that you exceed the size of a publish. This is all taken care of by the library.

In addition to simple quick and full wake cycles, the library supports the concept of a data capture function. This function is called according to a schedule, such as every 30 seconds, or even more complicated scenarios. The difference is that the library will adjust the sleep timing so the data capture function is called, and also continues to call the function if the device is already connecting, or attempting to connect ot the cloud. This assures consistent data acquisition regardless of cellular conditions. The data is saved in the flash file system and is uploaded in a data operation efficient manner, explained below.

### Event history

Event history allows small chunks of JSON data to be saved. For example, the data capture example above stores a timestamp (32 bit integer) and a floating point temperature value (with one decimal place). 

The event history data is uploaded as an array in an event, so it's data operation efficient. Many small data points can be uploaded in a single event. Additionally, the event history is combined with the wake event data, so you may be able to get both the wake event and event history data uploaded using a single data operation.

```json
{"soc":8.6,"ttc":16183,"wr":4,"eh":[{"b":1653305824}]}
```

This is an example of one button press in the 02-button example that occurred as a quick wake with no cloud connection. When the full wake occurred later, the eh (event history) key included the b event with the timestamp of the button press.

```json
{"soc":8.4,"ttc":12094,"rr":0,"eh":[{"b":1653307929},{"b":1653307941},{"b":1653307950}]}
```

This example contains three button presses. If you had so many button presses that it could not fit in a single event, it will automatically overflow into multiple events, but the default representation is data-efficient and can typically upload all of the data using only a single data operation.


### Scheduling

The underlying Device OS sleep API is relative; you specify the amount of time to sleep, but this is not always the most useful. This library works using a time schedule for when to capture or upload data to the cloud. 

Additionally, for devices in a fixed location, it's possible to configure time calculations to be in local time, including automatic support for daylight saving time. Otherwise, time calculation can be done at UTC.

```cpp
// Full wake and publish every 15 minutes
SleepHelper::instance().getScheduleFull()
    .withMinuteOfHour(15);

// Data capture every 2 minutes
SleepHelper::instance().getScheduleDataCapture()
    .withMinuteOfHour(2);
```

In the simplest case, you might have something like this; full wake and publish every 15 minutes and data capture every 2 minutes. This does not require any time zone information. 


```cpp
SleepHelper::instance().getScheduleFull()
    .withMinuteOfHour(15, LocalTimeRange(LocalTimeHMS("09:00:00"), LocalTimeHMS("16:59:59")))
```

Or every 15 minutes, but only from 9:00 AM to 5:00 PM (local time).

```cpp
SleepHelper::instance().getScheduleFull()
    .withMinuteOfHour(15, LocalTimeRange(LocalTimeHMS("09:00:00"), LocalTimeHMS("16:59:59")))
    .withHourOfDay(2);
```

Or every 15 minutes from 9:00 AM to 5:00 PM (local time), and every 2 hours the rest of the day.

The scheduling is significantly more powerful than this; see the [LocalTimeRK](https://github.com/rickkas7/LocalTimeRK) library for more information.

### State machines

The library is built as multiple finite state machines. One manages the cellular connection. Another handles the data capture functions, which is why data capture continues independent of whether you're connected to cellular or not, or attempting to connect.

In most cases you will perform a quick non-blocking data from your callback functions. However, certain callback functions are designed so you can perform lengthy operations in your own state machine.

These callback are passed a modifiable `AppCallbackState` object and include:

- withDataCaptureFunction()
- withSleepReadyFunction()
- withNoConnectionFunction()
- withMinimumConnectedTime()

For example, with the sleep ready function, your callback:

- Returns false if your situation is OK to sleep now. This does not guaranteed that sleep will actually occur, because there can be many sleep ready functions and other calculations. Once you return false your callback will no longer be called until the next wake cycle.
- Returns true if you still have things to do before it's OK to sleep. You callback will continue to be called until it returns false.

The declaration for `AppCallbackState` is:

```cpp
class AppCallbackState {
public:
    static const int CALLBACK_STATE_START = -1;
    static const int CALLBACK_START_RETURNED_FALSE = -2;

    int callbackState = CALLBACK_STATE_START;
    void *callbackData = 0; //!< Callback can store data here
};
```

For example, in the sleep ready function, in each sleep cycle, the state will start at CALLBACK_STATE_START. Your callback is free to set the `callbackState` to any positive value so you can implement your own state machine. You can also store data in the `callbackData` pointer, if desired, or you can just store it in your own class or mutable lambda capture value.


## Callback functions

You can find the callback functions you can register functions for in the [browsable HTML documentation](https://rickkas7.github.io/SleepHelper/group__callbacks.html).

## Persistent data

There is a system for storing persistent data in retained memory, EEPROM, file system, etc. This is in its own topic: [Persistent Data](PersistentData.md).

## Cloud-based configuration

While the device-side code is in the library, the server-side code has not been written yet. When complete, this option feature will work like this:

Cloud-based configuration allows JSON settings to be stored and configured on the cloud-side. When a device connects to the cloud, it will periodically ask the cloud if there is a new configuration, in case the configuration was updated while the device was asleep. This is done by sending a hash of the current settings so it's data-efficient.

If the cloud has newer settings, it will send them by a function call to the device, which will update the settings stored in the flash file system.

Code can register a callback function to be notified if the settings change.

Settings include fleet defaults, group defaults, and device-specific settings.

## Maximum connection time

Some examples use a maximum time to connect:

```cpp
// EXAMPLE
SleepHelper::instance()
    .withShouldConnectMinimumSoC(9.0)
    .withMaximumTimeToConnect(11min);

// PROTOTYPE
SleepHelper &withMaximumTimeToConnect(std::chrono::milliseconds timeMs); 
```

If the cloud connection starts but does not successfully complete, this can be safely done at 11 to 12 minutes. The reason is that around 10 minutes, the modem will be powered down, which can clear some temporary conditions in the modem.

However, if you have a battery-sensitive situation (only battery or battery with solar), then you may not want to wait the full 11 minutes. As long as you are using sleep mode, and using it will cellular off, that is sufficient to reset the modem in the same way, so you can use a lower value, possibly as low as 4 minutes. If you are using a 2G/3G device cellular device, you may want to set it a bit longer, 5 to 6 minutes.

If you use this technique to reduce the maximum time to connect, makes sure that you do not set withMinimumCellularOffTime, or set it to a value long enough to assure that the modem will be powered off to make sure it is reset. 

## Examples

### 01-simple example

This is a minimal example that just shows how the callback functions are registered. It doesn't really do anything interesting.

### 02-button example

This example shows how to both quick and full wakes, and customized sleep.

A momentary switch is connected between pin D2 and ground. Every time the button is pressed, a timestamp is logged. You could imagine the same technique being used for a door sensor, vehicle sensor, rain gauge, etc..

```cpp
SleepHelper::instance()
    .withMinimumCellularOffTime(5min);
```

This example optimizes the cellular modem to avoid aggressive reconnection. If the cellular modem would be off for less than 5 minutes, the library will instead use network standby mode to stay connected to cellular. This uses more power keeping the modem powered, but it also saves power by using less power to reconnect and having a nearly instantaneous reconnection time.

```cpp
SleepHelper::instance()
    .withSleepConfigurationFunction([](SystemSleepConfiguration &sleepConfig, SleepHelper::SleepConfigurationParameters &params) {
        // Add a GPIO wake on button press
        sleepConfig.gpio(BUTTON_PIN, FALLING);
        return true;
    });
```    

Since this example wakes by GPIO pin, it uses a sleep configuration function to add GPIO wake to the `SystemSleepConfiguration` before sleep.


```cpp
SleepHelper::instance()
    .withWakeFunction([](const SystemSleepResult &sleepResult) {
        if (sleepResult.wakeupReason() == SystemSleepWakeupReason::BY_GPIO) {
            pin_t whichPin = sleepResult.wakeupPin();
            Log.info("wake by pin %d", whichPin);
            if (whichPin == BUTTON_PIN) {
                logButtonPress();
                lastButtonPress = pinWakeMillis = millis();
            }
            else {
                pinWakeMillis = 0;
            }
        }
        return true;
    });
```

In order to handle wake from GPIO pin, a wake function is also added. If the wake occurred from the button GPIO pin, this is logged. 


```cpp
SleepHelper::instance()
    .withShouldConnectFunction([](int &connectConviction, int &noConnectConviction) {
        if (pinWakeMillis) {
            // We probably don't want to connect if woken by pin
            noConnectConviction = 60;
        }
        return true;
    });
```

In this example we don't want to do a full wake on button press unless it's also the scheduled time to do a full wake. Using a should connect handler allows this behavior to be fine-tuned.

```cpp
SleepHelper::instance()
    .withNoConnectionFunction([](SleepHelper::AppCallbackState &state) {
        // If woken by pin, wait until button is released
        if (pinWakeMillis) {
            // return true to stay awake, false to allow sleep
            return (digitalRead(BUTTON_PIN) == LOW);
        }
        else {
            return false;
        }
    });
```

One minor thing: If doing a quick wake, the button cloud still be pressed when the device is ready to go to sleep. This special case is handled here. If the button is still pressed, it will wait for it to be released before trying to go to sleep.

```cpp
// Full wake and publish every 15 minutes
SleepHelper::instance().getScheduleFull()
    .withMinuteOfHour(15);
```

It's configured to connect to the cloud every 15 minutes and upload the timestamps of the button presses.

```cpp
void logButtonPress() {
    Log.info("button press");
    SleepHelper::instance().addEvent([](JSONWriter &writer) {
        writer.name("b").value(Time.isValid() ? Time.now() : 0);
    });
}
```

Finally, button presses are logged using the Event History feature. Event History is data-efficient because it will store all of the events in mini-events, saved into a JSON array. This allows many data points to be uploaded via a single Particle.publish, saving data operations and also speeding the upload, since publishes are rate limited to approximately one per second.

Event history also supports overflow, so if the data exceeds the publish limit of 1024 bytes, it will be spread across multiple events as necessary. This happens automatically.



### 03-temperature example

This example shows how to use data collection mode to periodically sense temperature. The actual code uses a TMP36 analog temperature sensor, but you could use it with any sensor by replacing the readTempC() function, and of course you could sample other kinds of data other than temperature. 

```cpp
SleepHelper::instance()
    .withDataCaptureFunction([](SleepHelper::AppCallbackState &state) {
        if (Time.isValid()) {
            SleepHelper::instance().addEvent([](JSONWriter &writer) {
                writer.name("t").value((int) Time.now());
                writer.name("c").value(readTempC(), 1);
            });
        }
        return false;
    });
```

The most important thing in this example is the data capture function. It's called on a schedule to sample data, and this can occur after a quick wake (no cellular connection), while connecting, or while connected to the cloud. 

This example stores a timestamp ("t") and a temperature value ("c") but since it's JSON you could store more key/value pairs. For example, you might store temperature and humidity.


```cpp
// Full wake and publish every 15 minutes
SleepHelper::instance().getScheduleFull()
    .withMinuteOfHour(15);

// Data capture every 2 minutes
SleepHelper::instance().getScheduleDataCapture()
    .withMinuteOfHour(2);
```

This schedule is simple: full wake and publish every 15 minutes, and capture temperature every 2 minutes. Since the schedule uses only minute of hour, this does not require a valid timezone to be set.

## Version History

### 0.0.4 (2022-07-15)


### 0.0.3 (2022-06-21)

- Updated to PublishQueuePosix 0.0.4 to fix two bugs when using SleepHelper and PublishQueuePosixRK at the same time:
  - Events in the file queue would not be sent in some cases
  - After sending events, the device would not go back to sleep in some cases


### 0.0.2 (2022-06-01)

- Implementation of withPublishQueuePosixRK and withAB1805_WDT. Example usage in more-examples/50-publish-queue.
- Added a new section "Maximum connection time" that describes how to set the value. 

### 0.0.1 (2022-05-23)

- Initial version (incomplete)