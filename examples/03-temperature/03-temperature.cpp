
#include "SleepHelper.h"

Serial1LogHandler logHandler(115200, LOG_LEVEL_INFO);

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

const pin_t TMP36_SENSE_PIN = A0;
const pin_t TMP36_POWER_PIN = A1;

float readTempC();

void setup() {
    pinMode(TMP36_POWER_PIN, OUTPUT);
    digitalWrite(TMP36_POWER_PIN, LOW);

    SleepHelper::instance()
        .withMinimumCellularOffTime(5min)
        .withMaximumTimeToConnect(11min)
        .withDataCaptureFunction([](SleepHelper::AppCallbackState &state) {
            if (Time.isValid()) {
                SleepHelper::instance().addEvent([](JSONWriter &writer) {
                    writer.name("t").value((int) Time.now());
                    writer.name("c").value(readTempC(), 1);
                });
            }
            return false;
        })
        .withTimeConfig("EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00")
        .withEventHistory("/usr/events.txt", "eh");

    // Full wake and publish every 15 minutes
    SleepHelper::instance().getScheduleFull()
        .withMinuteOfHour(15);

    // Data capture every 2 minutes
    SleepHelper::instance().getScheduleDataCapture()
        .withMinuteOfHour(2);

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();
}

float readTempC() {
    digitalWrite(TMP36_POWER_PIN, HIGH);
    delay(2);

    int adcValue = analogRead(TMP36_SENSE_PIN);
    digitalWrite(TMP36_POWER_PIN, LOW);

    // Analog inputs have values from 0-4095, or
    // 12-bit precision. 0 = 0V, 4095 = 3.3V, 0.0008 volts (0.8 mV) per unit
    // The temperature sensor docs use millivolts (mV), so use 3300 as the factor instead of 3.3.
    float mV = ((float)adcValue) * 3300 / 4095;

    // According to the TMP36 docs:
    // Offset voltage 500 mV, scaling 10 mV/deg C, output voltage at 25C = 750 mV (77F)
    // The offset voltage is subtracted from the actual voltage, allowing negative temperatures
    // with positive voltages.

    // Example value=969 mV=780.7 tempC=28.06884765625 tempF=82.52392578125

    // With the TMP36, with the flat side facing you, the pins are:
    // Vcc | Analog Out | Ground
    // You must put a 0.1 uF capacitor between the analog output and ground or you'll get crazy
    // inaccurate values!

    // As configured above, connect VCC to A1 and Analog Out to A0.

    float tempC = (mV - 500) / 10;

    return tempC;
}

#if 0
        .withWakeEventFunction([](JSONWriter &writer, int &priority) {
            FuelGauge fuel;
            writer.name("volt").value(fuel.getVCell());
            writer.name("soc").value(fuel.getSoC());
            priority = 50;
            return false;
        })        

#endif