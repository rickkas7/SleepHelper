#include "DebounceSwitchRK.h"
#include "LocalTimeRK.h"

#include "SleepHelper.h"

Serial1LogHandler logHandler(115200, LOG_LEVEL_INFO);

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

const pin_t BUTTON_PIN = D2;
bool wokeByPin = false;

void setup() {
    // For counting button clicks while awake
    DebounceSwitch::getInstance()->setup();
    DebounceSwitch::getInstance()->addSwitch(BUTTON_PIN, DebounceSwitchStyle::PRESS_LOW_PULLUP, [](DebounceSwitchState *switchState, void *context) {
        Log.info("pin=%d state=%s", switchState->getPin(), switchState->getPressStateName());
        if (switchState->getPressState() == DebouncePressState::TAP) {
            Log.info("%d taps", switchState->getTapCount());
        }    
    });

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
                    wokeByPin = true;
                }
                else {
                    wokeByPin = false;
                }
            }
            return true;
        })
        .withShouldConnectFunction([](int &connectConviction, int &noConnectConviction) {
            if (wokeByPin) {
                noConnectConviction = 100;
            }
            return true;
        })
        .withMaximumTimeToConnect(11min)
        .withTimeConfig("EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00");

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();
}
