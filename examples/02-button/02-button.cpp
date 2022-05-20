#include "LocalTimeRK.h"

#include "SleepHelper.h"

Serial1LogHandler logHandler(115200, LOG_LEVEL_INFO);

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

const pin_t BUTTON_PIN = D2;
system_tick_t pinWakeMillis = 0;
system_tick_t lastButtonPress = 0;

void logButtonPress();

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    SleepHelper::instance()
        .withShouldConnectMinimumSoC(9.0)
        .withMinimumCellularOffTime(5min)
        .withSleepConfigurationFunction([](SystemSleepConfiguration &sleepConfig, SleepHelper::SleepConfigurationParameters &params) {
            // Add a GPIO wake on button press
            sleepConfig.gpio(BUTTON_PIN, FALLING);
            return true;
        })
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
        .withShouldConnectFunction([](int &connectConviction, int &noConnectConviction) {
            if (pinWakeMillis) {
                // We probably don't want to connect if woken by pin
                noConnectConviction = 60;
            }
            return true;
        })
        .withNoConnectionFunction([](SleepHelper::AppCallbackState &state) {
            // If woken by pin, wait until button is released
            if (pinWakeMillis) {
                // return true to stay awake, false to allow sleep
                return (digitalRead(BUTTON_PIN) == LOW);
            }
            else {
                return false;
            }
        })
        .withMaximumTimeToConnect(11min)
        .withTimeConfig("EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00")
        .withEventHistory("/usr/events.txt", "eh");

    // Full wake and publish every 15 minutes
    SleepHelper::instance().getScheduleFull()
        .withMinuteOfHour(15);

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();

    if (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - lastButtonPress >= 1000) {
            logButtonPress();
        }
        lastButtonPress = millis();
    }

}


void logButtonPress() {
    Log.info("button press");
    SleepHelper::instance().addEvent([](JSONWriter &writer) {
        writer.name("b").value(Time.isValid() ? Time.now() : 0);
    });
}
