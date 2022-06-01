// These must be included before SleepHelper.h!
#include "AB1805_RK.h"
#include "PublishQueuePosixRK.h"

#include "SleepHelper.h"


Serial1LogHandler logHandler(115200, LOG_LEVEL_INFO, {
	{ "app.pubq", LOG_LEVEL_TRACE },    // Add additional logging for PublishQueuePosixRK
	{ "app.seqfile", LOG_LEVEL_TRACE }  // And the underlying sequential file library used by PublishQueuePosixRK
});


SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

AB1805 ab1805(Wire);


void setup() {
    // Initialize AB1805 Watchdog and RTC
    {
        ab1805.setup();

        // Reset the AB1805 configuration to default values
        ab1805.resetConfig();

        // Enable watchdog
        ab1805.setWDT(AB1805::WATCHDOG_MAX_SECONDS);

        // If not using a supercap, comment out this line to disable trickle charging it
        ab1805.setTrickle(AB1805::REG_TRICKLE_DIODE_0_3 | AB1805::REG_TRICKLE_ROUT_3K);
    }

    // Initialize PublishQueuePosixRK
	PublishQueuePosix::instance().setup();


    SleepHelper::instance()
        .withMinimumCellularOffTime(5min)
        .withMaximumTimeToConnect(11min)
        .withTimeConfig("EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00")
        .withEventHistory("/usr/events.txt", "eh")
        .withAB1805_WDT(ab1805) // Stop the watchdog before sleep or reset, and resume after wake
        .withPublishQueuePosixRK() // Manage both internal publish queueing and PublishQueuePosixRK
        ;

    // Full wake and publish
    // - Every 15 minutes from 9:00 AM to 5:00 PM local time on weekdays (not Saturday or Sunday)
    // - Every 2 hours other times
    SleepHelper::instance().getScheduleFull()
        .withMinuteOfHour(15, LocalTimeRange(LocalTimeHMS("09:00:00"), LocalTimeHMS("16:59:59"), LocalTimeRestrictedDate(LocalTimeDayOfWeek::MASK_WEEKDAY)))
        .withHourOfDay(2);

    SleepHelper::instance().setup();
}

void loop() {
    SleepHelper::instance().loop();

    ab1805.loop();
    PublishQueuePosix::instance().loop();
}

