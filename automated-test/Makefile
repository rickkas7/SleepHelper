

all : AutomatedTest
	export TZ='UTC' && ./AutomatedTest

AutomatedTest : AutomatedTest.cpp ../src/SleepHelper.cpp ../src/SleepHelper.h ../lib/LocalTimeRK/src/LocalTimeRK.cpp libwiringgcc
	gcc AutomatedTest.cpp ../src/SleepHelper.cpp ../lib/LocalTimeRK/src/LocalTimeRK.cpp gcclib/libwiringgcc.a -DUNITTEST -std=c++11 -lc++ -Igcclib -I../src -I../lib/LocalTimeRK/src -o AutomatedTest

check : AutomatedTest.cpp ../src/SleepHelper.cpp ../src/SleepHelper.h libwiringgcc
	gcc AutomatedTest.cpp ../src/SleepHelper.cpp gcclib/libwiringgcc.a -g -O0 -std=c++11 -lc++ -Igcclib -I ../src -o AutomatedTest && valgrind --leak-check=yes ./AutomatedTest 

libwiringgcc :
	cd gcclib && make libwiringgcc.a 	
	
.PHONY: libwiringgcc
