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
	
	const char *testPath = "settings1.json";

	{
		unlink(testPath);

		SleepHelper::SettingsFile settings;
		settings.withPath(testPath);
		settings.load();

		String keyChanged;
		bool bResult;
		int intValue;
		bool boolValue;
		double doubleValue;
		String stringValue;

		settings.withSettingChangeFunction([&keyChanged](const char *key) {
			// printf("setting changed %s!\n", key);
			keyChanged = key;
			return true;
		});

		assertStr("", keyChanged, "");

		settings.setValue("t1", 1234);
		assertStr("", keyChanged, "t1");

		settings.setValue("t2", "testing 2!");
		assertStr("", keyChanged, "t2");

		settings.setValue("t3", -5.5);
		assertStr("", keyChanged, "t3");

		settings.setValue("t4", false);
		assertStr("", keyChanged, "t4");

		bResult = settings.getValue("t1", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 1234);

		bResult = settings.getValue("t2", stringValue);
		assertInt("", bResult, true);
		assertStr("", stringValue, "testing 2!");

		bResult = settings.getValue("t3", doubleValue);
		assertInt("", bResult, true);
		assertDouble("", doubleValue, -5.5, 0.001);

		bResult = settings.getValue("t4", boolValue);
		assertInt("", bResult, true);
		assertInt("", boolValue, false);


		settings.updateValuesJson("{\"t1\":9999}");
		assertStr("", keyChanged, "t1");
		keyChanged = "";

		settings.updateValuesJson("{\"t1\":9999}");
		assertStr("", keyChanged, "");

		SleepHelper::SettingsFile settings2;
		settings2.withPath(testPath);
		settings2.load();

		intValue = 0;
		bResult = settings2.getValue("t1", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 9999);

		stringValue = "";
		bResult = settings2.getValue("t2", stringValue);
		assertInt("", bResult, true);
		assertStr("", stringValue, "testing 2!");

		doubleValue = 0;
		bResult = settings2.getValue("t3", doubleValue);
		assertInt("", bResult, true);
		assertDouble("", doubleValue, -5.5, 0.001);

		boolValue = true;
		bResult = settings2.getValue("t4", boolValue);
		assertInt("", bResult, true);
		assertInt("", boolValue, false);

		unlink(testPath);
	}

	{
		// Default values on initial set
		unlink(testPath);

		const char *defaultValues = "{\"t1\":1234,\"t2\":\"testing 2!\",\"t3\":-5.5,\"t4\":false}";

		SleepHelper::SettingsFile settings;
		settings.withPath(testPath);
		settings.withDefaultValues(defaultValues);
		settings.load();


		String keyChanged;
		bool bResult;
		int intValue;
		bool boolValue;
		double doubleValue;
		String stringValue;

		settings.withSettingChangeFunction([&keyChanged](const char *key) {
			// printf("setting changed %s!\n", key);
			keyChanged = key;
			return true;
		});

		assertStr("", keyChanged, "");

		settings.getValuesJson(stringValue);
		assertStr("", stringValue, defaultValues);


		bResult = settings.getValue("t1", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 1234);

		bResult = settings.getValue("t2", stringValue);
		assertInt("", bResult, true);
		assertStr("", stringValue, "testing 2!");

		bResult = settings.getValue("t3", doubleValue);
		assertInt("", bResult, true);
		assertDouble("", doubleValue, -5.5, 0.001);

		bResult = settings.getValue("t4", boolValue);
		assertInt("", bResult, true);
		assertInt("", boolValue, false);


		// Make sure default values do not override
		// Also new defaults are added
		SleepHelper::SettingsFile settings2;
		settings2.withPath(testPath);
		settings2.withDefaultValues("{\"t1\":999,\"t2\":\"testing!\",\"t3\":-3.1,\"t4\":true,\"t5\":555}");
		settings2.load();

		intValue = 0;
		bResult = settings2.getValue("t1", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 1234);

		stringValue = "";
		bResult = settings2.getValue("t2", stringValue);
		assertInt("", bResult, true);
		assertStr("", stringValue, "testing 2!");

		doubleValue = 0;
		bResult = settings2.getValue("t3", doubleValue);
		assertInt("", bResult, true);
		assertDouble("", doubleValue, -5.5, 0.001);

		boolValue = true;
		bResult = settings2.getValue("t4", boolValue);
		assertInt("", bResult, true);
		assertInt("", boolValue, false);

		intValue = 0;
		bResult = settings2.getValue("t5", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 555);


		unlink(testPath);

	}

	// Cloud Settings
	{
		unlink(testPath);

		const char *cloudSettings = "{\"t1\":1234,\"t2\":\"testing 2!\",\"t3\":-5.5,\"t4\":false}";

		SleepHelper::CloudSettingsFile settings;
		settings.withPath(testPath);
		settings.load();

		settings.setValuesJson(cloudSettings);

		String keyChanged;
		bool bResult;
		int intValue;
		bool boolValue;
		double doubleValue;
		String stringValue;

		settings.withSettingChangeFunction([&keyChanged](const char *key) {
			// printf("setting changed %s!\n", key);
			keyChanged = key;
			return true;
		});

		assertStr("", keyChanged, "");

		settings.getValuesJson(stringValue);
		assertStr("", stringValue, cloudSettings);


		bResult = settings.getValue("t1", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 1234);

		bResult = settings.getValue("t2", stringValue);
		assertInt("", bResult, true);
		assertStr("", stringValue, "testing 2!");

		bResult = settings.getValue("t3", doubleValue);
		assertInt("", bResult, true);
		assertDouble("", doubleValue, -5.5, 0.001);

		bResult = settings.getValue("t4", boolValue);
		assertInt("", bResult, true);
		assertInt("", boolValue, false);

		assertInt("", (int)settings.getHash(), 1924270570);

		const char *cloudSettings2 = "{\"t1\":9999,\"t2\":\"testing 2!\",\"t3\":-5.5,\"t4\":false}";
		settings.setValuesJson(cloudSettings2);
		assertStr("", keyChanged, "t1");

		bResult = settings.getValue("t1", intValue);
		assertInt("", bResult, true);
		assertInt("", intValue, 9999);

		assertInt("", (int)settings.getHash(), 109685353);

		unlink(testPath);
	}


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

		data.setValue_lastUpdateCheck(123);
	}

	{
	}

	unlink(persistentDataPath);
}

class MyPersistentData : public SleepHelper::PersistentDataFile {
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

	MyPersistentData() : PersistentDataFile(&myData.header, sizeof(MyData), DATA_MAGIC, DATA_VERSION) {};

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

	MyPersistentData data;
	data.withPath(persistentDataPath);
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


	MyPersistentData data2;
	data2.withPath(persistentDataPath);
	data2.load();

	assertInt("", data2.getValue_test1(), 0x55aa55aa);
	assertInt("", data2.getValue_test2(), true);
	assertDouble("", data2.getValue_test3(), 9999999.12345, 0.001);
	assertStr("", data2.getValue_test4(), "testing1!");

	unlink(persistentDataPath);

}


class RetainedDataTest : public SleepHelper::PersistentDataBase {
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

	static const uint32_t DATA_MAGIC = 0xd971e39b;
	static const uint16_t DATA_VERSION = 1;

	RetainedDataTest(SleepHelper::PersistentDataBase::SavedDataHeader *header) : SleepHelper::PersistentDataBase(header, sizeof(MyData), DATA_MAGIC, DATA_VERSION) {};

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

};

void customRetainedDataTest() {

	RetainedDataTest::MyData retainedData; // Simulating retained data


	RetainedDataTest data(&retainedData.header);
	bool bResult;
	String s;

	data.load();

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


	RetainedDataTest data2(&retainedData.header);
	data2.load();

	assertInt("", data2.getValue_test1(), 0x55aa55aa);
	assertInt("", data2.getValue_test2(), true);
	assertDouble("", data2.getValue_test3(), 9999999.12345, 0.001);
	assertStr("", data2.getValue_test4(), "testing1!");

}


void eventCombinerTest() {
	{
		SleepHelper::EventCombiner t1;
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 60;
			return true;
		});
		t1.withCallback([](JSONWriter &jw, int &priority) {
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
		t1.withCallback([](JSONWriter &jw, int &priority) {
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

	{
		// Dedeupe one-time processed most recently added first at same priority
		SleepHelper::EventCombiner t1;
		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 60;
			return true;
		});
		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(9999);
			priority = 60;
			return true;
		});
		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("b").value(true);
			priority = 60;
			return true;
		});
		std::vector<String> events;

		t1.generateEvents(events, 18);
		assertInt("", events.size(), 2);
		assertStr("", events[0].c_str(), "{\"b\":true}");
		assertStr("", events[1].c_str(), "{\"a\":9999}");

	}	

	{
		// Dedeupe complex - high priority first, multiple keys
		SleepHelper::EventCombiner t1;
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 60;
			return true;
		});
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(9999);
			jw.name("b").value("test");
			priority = 70;
			return true;
		});
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("b").value("xxx");
			priority = 60;
			return true;
		});
		std::vector<String> events;

		t1.generateEvents(events, 32);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":9999,\"b\":\"test\"}");

	}	
	// One-time callback functions

	{
		// Generate two events
		SleepHelper::EventCombiner t1;
		t1.withCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 60;
			return true;
		});
		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("b").value(true);
			priority = 60;
			return true;
		});
		std::vector<String> events;

		t1.generateEvents(events, 18);
		assertInt("", events.size(), 2);
		assertStr("", events[0].c_str(), "{\"b\":true}");
		assertStr("", events[1].c_str(), "{\"a\":123}");

		t1.generateEvents(events, 18);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":123}");

	}
}

void eventHistoryTest() {
	// JSONCopy tests (low level)
	{
		const char *t1 = "{\"a\":123}";

		char buf[256];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		writer.beginObject();
		writer.name("x");
		SleepHelper::JSONCopy(t1, writer);
		writer.endObject();

		assertStr("", buf, "{\"x\":{\"a\":123}}");
	}
	{
		const char *t1 = "{\"a\":123,\"b\":true}";

		char buf[256];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		writer.beginObject();
		writer.name("x");
		SleepHelper::JSONCopy(t1, writer);
		writer.endObject();

		assertStr("", buf, "{\"x\":{\"a\":123,\"b\":true}}");
	}

	{
		const char *t1 = "{\"a\":123,\"b\":true,\"c\":\"testing\"}";

		char buf[256];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		writer.beginArray();
		SleepHelper::JSONCopy(t1, writer);
		writer.endArray();

		assertStr("", buf, "[{\"a\":123,\"b\":true,\"c\":\"testing\"}]");
	}
	{
		const char *t1 = "{\"a\":123,\"b\":true,\"d\":null,\"e\":-5.5}";

		char buf[256];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		writer.beginObject();
		writer.name("x");
		SleepHelper::JSONCopy(t1, writer);
		writer.endObject();

		assertStr("", buf, "{\"x\":{\"a\":123,\"b\":true,\"d\":null,\"e\":-5.5}}");
	}
	{
		const char *t1 = "{\"a\":123,\"b\":true,\"d\":null,\"e\":-5.5,\"f\":[1,2,3]}";

		char buf[256];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		writer.beginObject();
		writer.name("x");
		SleepHelper::JSONCopy(t1, writer);
		writer.endObject();

		assertStr("", buf, "{\"x\":{\"a\":123,\"b\":true,\"d\":null,\"e\":-5.5,\"f\":[1,2,3]}}");
	}
	{
		const char *t1 = "{\"a\":123,\"b\":true,\"d\":null,\"e\":-5.5,\"f\":[1,2,3],\"g\":{\"h\":9999}}";

		char buf[256];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		writer.beginObject();
		writer.name("x");
		SleepHelper::JSONCopy(t1, writer);
		writer.endObject();

		assertStr("", buf, "{\"x\":{\"a\":123,\"b\":true,\"d\":null,\"e\":-5.5,\"f\":[1,2,3],\"g\":{\"h\":9999}}}");
	}

	const char *eventsFile = "./events.txt";
	unlink(eventsFile);

	{
		// EventHistory: Simple  test
		SleepHelper::EventHistory events;
		events.withPath(eventsFile);

		events.addEvent("{\"a\":123}");
		assertFile("", eventsFile, "testfiles/events01.txt");

		events.addEvent("{\"a\":222}");
		assertFile("", eventsFile, "testfiles/events02.txt");

		char buf[1024];
		memset(buf, 0, sizeof(buf));
		JSONBufferWriter writer(buf, sizeof(buf) - 1);
		
		bool bResult = events.getEvents(writer, sizeof(buf));
        assertInt("", bResult, true);

		assertStr("", buf, "[{\"a\":123},{\"a\":222}]");

		bResult = events.getEvents(writer, sizeof(buf));
        assertInt("", bResult, false);

		struct stat sb;
		assertInt("", stat(eventsFile, &sb), -1);
		assertInt("", errno, ENOENT);

	}
	{
		// EventHistory: Get partial
		SleepHelper::EventHistory events;
		events.withPath(eventsFile);

		events.addEvent("{\"a\":123}");
		assertFile("", eventsFile, "testfiles/events01.txt");

		events.addEvent("{\"a\":222}");
		assertFile("", eventsFile, "testfiles/events02.txt");

		char buf[16];

		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);
			
			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, true);

			assertStr("", buf, "[{\"a\":123}]");
		}

		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);

			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, true);

			assertStr("", buf, "[{\"a\":222}]");
		}
		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);

			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, false);

			struct stat sb;
			assertInt("", stat(eventsFile, &sb), -1);
			assertInt("", errno, ENOENT);
		}

	}
	{
		// EventHistory: Get partial2
		SleepHelper::EventHistory events;
		events.withPath(eventsFile);

		events.addEvent("{\"a\":123}");
		events.addEvent("{\"a\":222}");
		events.addEvent("{\"a\":333}");

		char buf[26];

		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);
			
			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, true);

			assertStr("", buf, "[{\"a\":123},{\"a\":222}]");
		}

		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);

			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, true);

			assertStr("", buf, "[{\"a\":333}]");
		}
		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);

			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, false);

			struct stat sb;
			assertInt("", stat(eventsFile, &sb), -1);
			assertInt("", errno, ENOENT);
		}

	}
	{
		// EventHistory: Get partial 2 with separate remove and add between get and remove
		SleepHelper::EventHistory events;
		events.withPath(eventsFile);

		events.addEvent("{\"a\":123}");
		events.addEvent("{\"a\":222}");

		char buf[26];

		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);
			
			bool bResult = events.getEvents(writer, sizeof(buf), false);
			assertInt("", bResult, true);


			events.addEvent("{\"a\":333}");

			events.removeEvents();

			assertStr("", buf, "[{\"a\":123},{\"a\":222}]");
		}

		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);

			bool bResult = events.getEvents(writer, sizeof(buf), false);
			assertInt("", bResult, true);
			
			events.removeEvents();

			assertStr("", buf, "[{\"a\":333}]");
		}
		{
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);

			bool bResult = events.getEvents(writer, sizeof(buf), false);
			assertInt("", bResult, false);

			struct stat sb;
			assertInt("", stat(eventsFile, &sb), -1);
			assertInt("", errno, ENOENT);
		}

	}
	{
		// Event history - 100 single events
		SleepHelper::EventHistory events;
		events.withPath(eventsFile);

		// This is enough bytes to force multiple 512 byte buffer copies
		for(int ii = 0; ii < 100; ii++) {
			char json[256];
			snprintf(json, sizeof(json), "{\"a\":%d}", ii);
			events.addEvent(json);
		}


		for(int ii = 0; ii < 100; ii++) {
			char buf[16];
			memset(buf, 0, sizeof(buf));
			JSONBufferWriter writer(buf, sizeof(buf) - 1);
			
			bool bResult = events.getEvents(writer, sizeof(buf));
			assertInt("", bResult, true);

			char json[256];
			snprintf(json, sizeof(json), "[{\"a\":%d}]", ii);

			assertStr("", buf, json);
		}
	}

	// EventCombiner + EventHistory
	{
		SleepHelper::EventCombiner t1;
		t1.withEventHistory(eventsFile, "eh");

		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		t1.addEvent("{\"b\":123}");


		std::vector<String> events;
		t1.generateEvents(events, 50);
		assertInt("", events.size(), 1);
		assertStr("", events[0].c_str(), "{\"a\":123,\"eh\":[{\"b\":123}]}");
	}
	{
		// Too many events to fit in the combiner
		SleepHelper::EventCombiner t1;
		t1.withEventHistory(eventsFile, "eh");

		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		
		t1.addEvent("{\"b\":1111}");
		t1.addEvent("{\"b\":2222}");
		t1.addEvent("{\"b\":3333}");
		
		std::vector<String> events;
		t1.generateEvents(events, 24);
		assertInt("", events.size(), 4);
		assertStr("", events[0].c_str(), "{\"a\":123}");
		assertStr("", events[1].c_str(), "{\"eh\":[{\"b\":1111}]}");
		assertStr("", events[2].c_str(), "{\"eh\":[{\"b\":2222}]}");
		assertStr("", events[3].c_str(), "{\"eh\":[{\"b\":3333}]}");
	}

	{
		// Too many events to fit in the combiner
		SleepHelper::EventCombiner t1;
		t1.withEventHistory(eventsFile, "eh");

		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		
		t1.addEvent("{\"b\":1111}");
		t1.addEvent("{\"b\":2222}");
		t1.addEvent("{\"b\":3333}");
		
		std::vector<String> events;
		t1.generateEvents(events, 32);
		assertInt("", events.size(), 2);
		assertStr("", events[0].c_str(), "{\"a\":123,\"eh\":[{\"b\":1111}]}");
		assertStr("", events[1].c_str(), "{\"eh\":[{\"b\":2222},{\"b\":3333}]}");
	}

	{
		// addEvent JSONWriter
		SleepHelper::EventCombiner t1;
		t1.withEventHistory(eventsFile, "eh");

		t1.withOneTimeCallback([](JSONWriter &jw, int &priority) {
			jw.name("a").value(123);
			priority = 10;
			return true;
		});
		
		t1.addEvent([](JSONWriter &writer) {
			writer.name("b").value(1111);
		});
		t1.addEvent([](JSONWriter &writer) {
			writer.name("c").value(false);
		});
		t1.addEvent([](JSONWriter &writer) {
			writer.name("d").value("testing 1, 2, 3");
		});

		std::vector<String> events;
		t1.generateEvents(events, 40);
		//assertInt("", events.size(), 2);
		assertStr("", events[0].c_str(), "{\"a\":123,\"eh\":[{\"b\":1111},{\"c\":false}]}");
		assertStr("", events[1].c_str(), "{\"eh\":[{\"d\":\"testing 1, 2, 3\"}]}");
	}

	// unlink(eventsFile);
}


int main(int argc, char *argv[]) {
	settingsTest();
	persistentDataTest();
	customPersistentDataTest();
	customRetainedDataTest();
	eventCombinerTest();
	eventHistoryTest();
	return 0;
}
