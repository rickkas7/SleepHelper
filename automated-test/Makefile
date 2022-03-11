

all : AutomatedTest
	export TZ='UTC' && ./AutomatedTest

AutomatedTest : AutomatedTest.cpp ../src/SleepHelper.cpp ../src/SleepHelper.h ../lib/LocalTimeRK/src/LocalTimeRK.cpp ../lib/LocalTimeRK/src/LocalTimeRK.h ../lib/JsonParserGeneratorRK/src/JsonParserGeneratorRK.cpp ../lib/JsonParserGeneratorRK/src/JsonParserGeneratorRK.h libwiringgcc
	gcc AutomatedTest.cpp ../src/SleepHelper.cpp ../lib/LocalTimeRK/src/LocalTimeRK.cpp ../lib/JsonParserGeneratorRK/src/JsonParserGeneratorRK.cpp gcclib/libwiringgcc.a -DUNITTEST -std=c++11 -lc++ -Igcclib -I../src -I../lib/LocalTimeRK/src -I../lib/JsonParserGeneratorRK/src -o AutomatedTest

check : AutomatedTest.cpp ../src/SleepHelper.cpp ../src/SleepHelper.h libwiringgcc
	gcc AutomatedTest.cpp ../src/SleepHelper.cpp gcclib/libwiringgcc.a -g -O0 -std=c++11 -lc++ -Igcclib -I ../src -o AutomatedTest && valgrind --leak-check=yes ./AutomatedTest 

libwiringgcc :
	cd gcclib && make libwiringgcc.a 	
	
.PHONY: libwiringgcc
