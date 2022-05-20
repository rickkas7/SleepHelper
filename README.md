# SleepHelper

*Library for simplifying Particle applications that use sleep modes*

## Example

This is the simplest example, in the "01-simple" example directory:

```cpp
#include "SleepHelper.h"

SerialLogHandler logHandler;

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);


void setup() {
    SleepHelper::instance()
        .withShouldConnectMinimumSoC(9.0)
        .withMaximumTimeToConnect(11min);

    SleepHelper::instance().withSetupFunction([]() {
        Log.info("test setup!");
        return true;
    });

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();
}
```

Things to note in this code:

You must enable the system thread and use SEMI_AUTOMATIC system mode.

```cpp
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
```