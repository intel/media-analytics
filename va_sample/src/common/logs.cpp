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

#include <logs.h>

//  ========================================================================
// trace log utitlity

#define MAX_MSG_BUF_SIZE 1024

char g_MsgBuffer[MAX_MSG_BUF_SIZE];
int idx = 0;
int loglvl = LOGLEVEL;

int get_log_level()
{
    return loglvl;
}

int set_log_level(int level)
{
    loglvl = level;
    return loglvl;
}

void logheader(const char * format, ...)
{
    va_list args;
    va_start(args, format);
    idx = vsnprintf(g_MsgBuffer, MAX_MSG_BUF_SIZE, format, args);
    va_end(args);
}

void logmsg(const char * format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(&g_MsgBuffer[idx], MAX_MSG_BUF_SIZE, format, args);
    va_end(args);
    idx = 0;
    fprintf(stdout, "%s \n", g_MsgBuffer);
}

void errmsg(const char * format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(&g_MsgBuffer[idx], MAX_MSG_BUF_SIZE, format, args);
    va_end(args);
    idx = 0;
    fprintf(stderr, "%s \n", g_MsgBuffer);
}


void startTimer(stopWatch *timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

void stopTimer(stopWatch *timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->stop);
    timer->elapsed = (double)((1000 * 1000 * 1000 * timer->stop.tv_sec + timer->stop.tv_nsec)
        - (1000 * 1000 * 1000 * timer->start.tv_sec + timer->start.tv_nsec)) / 1000000;
}


void loglevel_setup()
{
    const char* s = getenv("LOGLEVEL");
    if (s)
    {
        printf(" ENV  LOGLEVEL %s ", s);
        int logv = strtol(s, NULL, 10);
        int ret = set_log_level(logv);
        printf("update LOGLEVEL to %d \n", ret);

    }
    else
        printf(" ENV LOGLEVEL not found, use default value %d. \n", loglvl);
}


