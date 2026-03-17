#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>

typedef uint32_t time_t;
typedef uint32_t clock_t;
#define CLOCKS_PER_SEC 1000000

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t* tloc);
clock_t clock(void);
struct tm* localtime(const time_t* timer);
struct tm* gmtime(const time_t* timer);
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm);
double difftime(time_t end, time_t start);

#endif
