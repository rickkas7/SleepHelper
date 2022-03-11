// Dummy particle.h file for testing logdata.cpp module from gcc
#ifndef __PARTICLE_H
#define __PARTICLE_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>

#include "spark_wiring_string.h"
#include "spark_wiring_time.h"
#include "rng_hal.h"

class Stream {
public:
	inline int available() { return 0; }
	inline int read() { return 0; }
};


// Doesn't actually work as we don't support threads in the gcclib, but makes it easier to compile code
class Mutex
{
public:
    /**
     * Creates a new mutex.
     */
    Mutex() {};

    ~Mutex() {
    }

    void lock() {  }
    bool trylock() { return true; }
    bool try_lock() { return true; }
    void unlock() { }

};

#define SINGLE_THREADED_SECTION()
#define SINGLE_THREADED_BLOCK()
#define WITH_LOCK(x)
#define TRY_LOCK(x)

class Logger {
public:
	Logger(const char *app) {};
};


#endif /* __PARTICLE_H */
