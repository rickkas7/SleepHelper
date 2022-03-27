#include "Particle.h"
#include "SleepHelper.h"


void readTestData(const char *filename, char *&data, size_t &size) {

	FILE *fd = fopen(filename, "r");
	if (!fd) {
		printf("failed to open %s\n", filename);
		return;
	}

	fseek(fd, 0, SEEK_END);
	size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	data = (char *) malloc(size + 1);
	fread(data, 1, size, fd);
	data[size] = 0;

	fclose(fd);
}

char *readTestData(const char *filename) {
	char *data = 0;
	size_t size;

	readTestData(filename, data, size);

	return data;
}

#define assertInt(msg, got, expected) _assertInt(msg, got, expected, __LINE__)
void _assertInt(const char *msg, int got, int expected, int line) {
	if (expected != got) {
		printf("assertion failed %s line %d\n", msg, line);
		printf("expected: %d\n", expected);
		printf("     got: %d\n", got);
		assert(false);
	}
}

#define assertDouble(msg, got, expected, margin) _assertDouble(msg, got, expected, margin, __LINE__)
void _assertDouble(const char *msg, double got, double expected, double margin, int line) {
	if ((expected < (got - margin)) || (expected > (got + margin))) {
		printf("assertion failed %s line %d\n", msg, line);
		printf("expected: %lf\n", expected);
		printf("     got: %lf\n", got);
		assert(false);
	}
}


#define assertStr(msg, got, expected) _assertStr(msg, got, expected, __LINE__)
void _assertStr(const char *msg, const char *got, const char *expected, int line) {
	if (strcmp(expected, got) != 0) {
		printf("assertion failed %s line %d\n", msg, line);
		printf("expected: %s\n", expected);
		printf("     got: %s\n", got);
		assert(false);
	}
}

#define assertTime(msg, got, expected) _assertTime(msg, got, expected, __LINE__)
void _assertTime(const char *msg, time_t got, const char *expected, int line) {
	struct tm timeInfo;
	LocalTime::timeToTm(got, &timeInfo);
	String gotStr = LocalTime::getTmString(&timeInfo);
	if (strcmp(expected, gotStr) != 0) {
		printf("assertion failed %s line %d\n", msg, line);
		printf("expected: %s\n", expected);
		printf("     got: %s\n", gotStr.c_str());
		assert(false);
	}
}

#define assertFile(msg, got, expected) _assertFile(msg, got, expected, __LINE__)
void _assertFile(const char *msg, const char *gotPath, const char *expectedPath, int line) {
	char *gotData, *expectedData;
	size_t gotSize, expectedSize;

	readTestData(gotPath, gotData, gotSize);
	readTestData(expectedPath, expectedData, expectedSize);

	if (gotSize != expectedSize) {
		printf("assertion failed %s line %d\n", msg, line);
		printf("expected size: %lu for %s\n", expectedSize, expectedPath);
		printf("     got size: %lu for %s\n", gotSize, gotPath);
		assert(false);
	}
	for(size_t ii = 0; ii < gotSize; ii++) {
		if (gotData[ii] != expectedData[ii]) {
			printf("assertion failed %s line %d\n", msg, line);
			printf("expected data: %02x index %lu for %s\n", expectedData[ii], ii, expectedPath);
			printf("     got data: %02x index %lu for %s\n", gotData[ii], ii, gotPath);
			assert(false);
		}
	}

	free(gotData);
	free(expectedData);
}


void settingsTest() {
	
	SleepHelper::SettingsFile settings;
	settings.withPath("./sleepSettings.json");
	settings.load();

	settings.withSettingChangeFunction([](const char *key) {
		printf("setting changed %s!\n", key);
		return true;
	});

	settings.setValue("t1", 1234);
	settings.setValue("t2", "testing 2!");
	settings.setValue("t3", -5.5);
	settings.setValue("t4", false);

	settings.setValuesJson("{\"t1\":9999}");
}

void persistentDataTest() {
	const char *persistentDataPath = "./temp01.dat";
	{
		SleepHelper::PersistentData data;
		unlink(persistentDataPath);
		data.withPath(persistentDataPath).withSaveDelayMs(0);
		data.load();
		data.save();

		assertFile("", persistentDataPath, "testfiles/test01.dat");

		data.setValue_nextUpdateCheck(123);
	}

	{
	}
}

class MyPersistentData : public SleepHelper::PersistentDataBase {
public:
	class MyData {
	public:
		// This structure must always begin with the header (20 bytes)
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

	MyPersistentData(const char *path) : PersistentDataBase(&myData.header, sizeof(MyData), path) {};

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


	MyData myData;
};

void customPersistentDataTest() {
	const char *persistentDataPath = "./temp02.dat";
	unlink(persistentDataPath);

	MyPersistentData data(persistentDataPath);
	bool bResult;
	String s;

	data.load();
	data.save();

	data.setValue_test1(0x55aa55aa);
	assertInt("", data.getValue_test1(), 0x55aa55aa);

	assertInt("", data.getValue_test2(), false);
	data.setValue_test2(true);
	assertInt("", data.getValue_test2(), true);


	assertDouble("", data.getValue_test3(), 0.0, 0.001);
	data.setValue_test3(9999999.12345);
	assertDouble("", data.getValue_test3(), 9999999.12345, 0.001);

	s = data.getValue_test4();
	assertStr("", s, "");
	data.setValue_test4("testing!"); 
	s = data.getValue_test4();
	assertStr("", s, "testing!");

	data.setValue_test4("testing1!"); 
	s = data.getValue_test4();
	assertStr("", s, "testing1!");

	bResult = data.setValue_test4("testing12!"); 
	assertInt("", bResult, false);
	s = data.getValue_test4();
	assertStr("", s, "testing1!");

	data.save();


	MyPersistentData data2(persistentDataPath);
	data2.load();

	assertInt("", data2.getValue_test1(), 0x55aa55aa);
	assertInt("", data2.getValue_test2(), true);
	assertDouble("", data2.getValue_test3(), 9999999.12345, 0.001);
	assertStr("", data2.getValue_test4(), "testing1!");


}

void eventCombinerTest() {
	{
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 16);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":123}");
	}
	{
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value("test");
			priority = 10;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 16);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":\"test\"}");
		// {"a":"test"}
		// 12345678901234567890
	}
	{
		// Just barely fits
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value("test12");
			priority = 10;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 16);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":\"test12\"}");
		// {"a":"test12"}
		// 12345678901234567890
	}
	{
		// Edge case
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value("test123");
			priority = 10;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 16);
		assertInt("", events.size(), 0);
	}
	{
		// Make sure you can't overflow the buffer if a single write is larger than the buffer
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value("test12345678");
			priority = 10;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 16);
		assertInt("", events.size(), 0);
	}
	{
		// Discard data
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("b").value(true);
			priority = 10;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 20);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":123,\"b\":true}");

		// {"a":123,"b":true}
		// 12345678901234567890

		t1.generateEvents(events, 18);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":123}");
	}
	{
		// Higher priority first with discard
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("b").value(true);
			priority = 20;
			return true;
		});
		std::vector<String> events;
		t1.generateEvents(events, 18);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"b\":true}");
	}
	{
		// Generate two events
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 60;
			return true;
		});
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("b").value(true);
			priority = 60;
			return true;
		});
		std::vector<String> events;

		t1.generateEvents(events, 18);
		assertInt("", events.size(), 2);
		assertStr("", events[0].c_str(), "{\"a\":123}");
		assertStr("", events[1].c_str(), "{\"b\":true}");
	}
	{
		// Complex event
		SleepHelper::EventCombiner t1;
		t1.withCallback([](spark::JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			jw.name("b").value("test");
			jw.name("c").value(true);
			jw.name("d").beginArray().value(1).value(2).value(3).endArray();
			priority = 60;
			return true;
		});
		std::vector<String> events;

		t1.generateEvents(events, 100);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":123,\"b\":\"test\",\"c\":true,\"d\":[1,2,3]}");

	}

	// 

}

int main(int argc, char *argv[]) {
	settingsTest();
	persistentDataTest();
	customPersistentDataTest();
	eventCombinerTest();
	return 0;
}
