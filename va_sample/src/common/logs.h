/*
* Copyright (c) 2019, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __LOGS_H__
#define __LOGS_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>

//  ========================================================================
// trace log utitlity
#define LOGLEVEL 0

typedef struct {
    struct timespec start;
    struct timespec stop;
    double elapsed;  // millisec
} stopWatch;

void startTimer(stopWatch *timer);
void stopTimer(stopWatch *timer);

int get_log_level();
int set_log_level(int level);
void logheader(const char * format, ...);
void logmsg(const char * format, ...);
void errmsg(const char * format, ...);
void loglevel_setup();

#define TRACE(_message, ...)     \
{ \
if (get_log_level() >1) \
   { \
        logheader("trace:  %s  %s( )  line%d:  ", __FILE__, __FUNCTION__, __LINE__); \
        logmsg(_message, ##__VA_ARGS__); \
    } \
}

#define INFO(_message, ...)     \
{ \
if (get_log_level() >0) \
   { \
        logheader("info:  %s  %s( )  line%d:  ", __FILE__, __FUNCTION__, __LINE__); \
        logmsg(_message, ##__VA_ARGS__); \
    } \
else \
    { \
        logmsg(_message, ##__VA_ARGS__); \
    } \
}

#define ERRLOG(_message, ...)     \
{ \
    logheader("error:  %s  %s( )  line%d:  ", __FILE__, __FUNCTION__, __LINE__); \
    errmsg(_message, ##__VA_ARGS__); \
}

#endif //__LOGS_H__
