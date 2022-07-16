// These must be included before SleepHelper.h!
#include "SpiffsParticleRK.h"

#include "SleepHelper.h"


SerialLogHandler logHandler(LOG_LEVEL_TRACE);


SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

// Chose a flash configuration:
SpiFlashISSI spiFlash(SPI, A2); 		// ISSI flash on SPI (A pins)
// SpiFlashISSI spiFlash(SPI1, D5);		// ISSI flash on SPI1 (D pins)
// SpiFlashMacronix spiFlash(SPI1, D5);	// Macronix flash on SPI1 (D pins), typical config for E series
// SpiFlashWinbond spiFlash(SPI, A2);	// Winbond flash on SPI (A pins)
// SpiFlashP1 spiFlash;					// P1 external flash inside the P1 module

// Create an object for the SPIFFS file system
SpiffsParticle spiffsFs(spiFlash);

class MyPersistentData : public SleepHelper::PersistentDataFileSystem {
public:
	class MyData {
	public:
		// This structure must always begin with the header (16 bytes)
		SleepHelper::PersistentDataBase::SavedDataHeader header;
		// Your fields go here. Once you've added a field you cannot add fields
		// (except at the end), insert fields, remove fields, change size of a field.
		// Doing so will cause the data to be corrupted!
		// You may want to keep a version number in your data.
		int test1;
		bool test2;
		double test3;
		char test4[10];
		// OK to add more fields here 
	};

	static const uint32_t DATA_MAGIC = 0x20a99e73;
	static const uint16_t DATA_VERSION = 1;

	MyPersistentData() : SleepHelper::PersistentDataFileSystem(new SleepHelper::FileSystemSpiffs(spiffsFs), &myData.header, sizeof(MyData), DATA_MAGIC, DATA_VERSION) {
        withFilename("test4.dat");
    };

	int getValue_test1() const {
		return getValue<int>(offsetof(MyData, test1));
	}

	void setValue_test1(int value) {
		setValue<int>(offsetof(MyData, test1), value);
	}

	bool getValue_test2() const {
		return getValue<bool>(offsetof(MyData, test2));
	}

	void setValue_test2(bool value) {
		setValue<bool>(offsetof(MyData, test2), value);
	}

	double getValue_test3() const {
		return getValue<double>(offsetof(MyData, test3));
	}

	void setValue_test3(double value) {
		setValue<double>(offsetof(MyData, test3), value);
	}

	String getValue_test4() const {
		String result;
		getValueString(offsetof(MyData, test4), sizeof(MyData::test4), result);
		return result;
	}
	bool setValue_test4(const char *str) {
		return setValueString(offsetof(MyData, test4), sizeof(MyData::test4), str);
	}

    void logData(const char *msg) {
        Log.info("%s: %d, %d, %lf, %s", msg, myData.test1, (int)myData.test2, myData.test3, myData.test4);
    }


	MyData myData;
};




void setup() {
    spiFlash.begin();
	spiffsFs.withPhysicalSize(64 * 1024);

	s32_t res = spiffsFs.mountAndFormatIfNecessary();
	Log.info("mount res=%d", (int)res);

    SleepHelper::instance()
        .withSleepEnabled(false)
        .setup();


}

void loop() {
    SleepHelper::instance().loop();

    static unsigned long lastCheck = 0;
    if (millis() - lastCheck >= 10000) {
        lastCheck = millis();
        const char *persistentDataPath = "test04.dat";

        MyPersistentData data;

        data.load();

        data.logData("after loading");

        data.setValue_test1(data.getValue_test1() + 1);
        data.setValue_test2(!data.getValue_test2());
        data.setValue_test3(data.getValue_test3() - 0.1);
        data.setValue_test4("testing!"); 

        data.logData("after update");

        data.flush(true);
    }  
}



