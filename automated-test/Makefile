

all : AutomatedTest
	export TZ='UTC' && ./AutomatedTest

AutomatedTest : AutomatedTest.cpp ../src/SleepHelper.cpp ../src/SleepHelper.h ../lib/LocalTimeRK/src/LocalTimeRK.cpp ../lib/LocalTimeRK/src/LocalTimeRK.h ../lib/JsonParserGeneratorRK/src/JsonParserGeneratorRK.cpp ../lib/JsonParserGeneratorRK/src/JsonParserGeneratorRK.h ../lib/StorageHelperRK/src/StorageHelperRK.cpp ../lib/StorageHelperRK/src/StorageHelperRK.h libwiringgcc
	gcc AutomatedTest.cpp ../src/SleepHelper.cpp ../lib/LocalTimeRK/src/LocalTimeRK.cpp ../lib/JsonParserGeneratorRK/src/JsonParserGeneratorRK.cpp ../lib/StorageHelperRK/src/StorageHelperRK.cpp unittestlib/libwiringgcc.a -DUNITTEST -std=c++11 -lc++ -Iunittestlib -I../src -I../lib/LocalTimeRK/src -I../lib/JsonParserGeneratorRK/src -I../lib/StorageHelperRK/src -o AutomatedTest

check : AutomatedTest.cpp ../src/SleepHelper.cpp ../src/SleepHelper.h libwiringgcc
	gcc AutomatedTest.cpp ../src/SleepHelper.cpp unittestlib/libwiringgcc.a -g -O0 -std=c++11 -lc++ -Iunittestlib -I ../src -o AutomatedTest && valgrind --leak-check=yes ./AutomatedTest 

libwiringgcc :
	cd unittestlib && make libwiringgcc.a 	
	
.PHONY: libwiringgcc
