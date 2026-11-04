#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../ports/logger.h"

int ERSWITCHER_DEBUG = 0;

static struct timespec start_time;
static int initialized = 0;

void logger_init(void) {
    if(initialized) return;

    const char* env = getenv("ERSWITCHER_DEBUG");
    ERSWITCHER_DEBUG = env && *env && strcmp(env, "0") != 0;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    initialized = 1;
}

void logger_debug(const char* format, ...) {
    if(!ERSWITCHER_DEBUG) return;
    if(!initialized) logger_init();

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long seconds = now.tv_sec - start_time.tv_sec;
    long nanoseconds = now.tv_nsec - start_time.tv_nsec;
    if(nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000L;
    }

    fprintf(stderr, "[%10ld.%06ld] ", seconds, nanoseconds / 1000);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}
