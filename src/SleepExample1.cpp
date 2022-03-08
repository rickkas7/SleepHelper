#include "SleepHelper.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

void setup() {
    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();
}

