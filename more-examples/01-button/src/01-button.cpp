#include "DebounceSwitchRK.h"
#include "LocalTimeRK.h"

#include "SleepHelper.h"

Serial1LogHandler logHandler(115200, LOG_LEVEL_INFO);

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

const pin_t BUTTON_PIN = D2;
system_tick_t pinWakeMillis = 0;

void logButtonPress();

void setup() {
    // For counting button clicks while awake
    /*
    DebounceSwitch::getInstance()->setup();
    DebounceSwitch::getInstance()->addSwitch(BUTTON_PIN, DebounceSwitchStyle::PRESS_LOW_PULLUP, [](DebounceSwitchState *switchState, void *context) {
        Log.info("pin=%d state=%s", switchState->getPin(), switchState->getPressStateName());
        switch(switchState->getPressState()) {
        case DebouncePressState::TAP:
            logButtonPress();
            break;

        case DebouncePressState::PRESS_START:
        case DebouncePressState::RELEASED:
        default:
            break;
        }
    });
    */

    SleepHelper::instance()
        .withShouldConnectMinimumSoC(9.0)
        .withSleepConfigurationFunction([](SystemSleepConfiguration &sleepConfig, std::chrono::milliseconds&duration) {
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
                    pinWakeMillis = millis();
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
        .withNoConnectionFunction([]() {
            // If woken by pin, wait 2 seconds for additional button presses (return true)
            // Then allow sleep (return false)
            if (pinWakeMillis) {
                return (millis() - pinWakeMillis < 2000);
            }
            else {
                return false;
            }
        })
        .withMaximumTimeToConnect(11min)
        .withTimeConfig("EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00")
        .withEventHistory("/usr/events.txt", "eh");

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();
}


void logButtonPress() {
    Log.info("button press");
    SleepHelper::instance().addEvent([](JSONWriter &writer) {
        writer.name("b").value(Time.isValid() ? Time.now() : 0);
    });
}
