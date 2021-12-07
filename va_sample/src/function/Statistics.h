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

#ifndef __STATISTICS_H__
#define __STATISTICS_H__
#include <stdint.h>
#include <stdio.h>
#include <mutex>
#include <vector>

enum StatisticsType
{
    DECODED_FRAMES = 0,
    INFERENCE_FRAMES_RECEIVED = 1,
    INFERENCE_FRAMES_PROCESSED = 2,
    INFERENCE_FRAMES_OD_RECEIVED = 3,
    INFERENCE_FRAMES_OD_PROCESSED = 4,
    INFERENCE_FRAMES_OC_RECEIVED = 5,
    INFERENCE_FRAMES_OC_PROCESSED = 6,
    STATISTICS_TYPE_NUM
};

class Statistics
{
public:
    static Statistics& getInstance()
    {
        static Statistics instance;
        return instance;
    }
    ~Statistics();

    void Step(StatisticsType type);

    void Update(StatisticsType type = STATISTICS_TYPE_NUM);

    void Report();

    void ReportPeriodly(float period, int duration = -1);

    void CountDownStart(int number) { m_countdown_counter = number; }

    void CountDown();

    inline bool IsNoFPS() { return (m_countersCurrentCycle[DECODED_FRAMES]==0 && m_countersCurrentCycle[INFERENCE_FRAMES_RECEIVED]==0); }

    bool IsStarted();

private:
    Statistics();

    std::vector<std::mutex> m_mutex;
    uint32_t m_countersCurrentCycle[STATISTICS_TYPE_NUM];
    uint32_t m_counters[STATISTICS_TYPE_NUM];
    uint64_t m_accCounters[STATISTICS_TYPE_NUM];
    uint32_t m_accNum[STATISTICS_TYPE_NUM];

    std::mutex m_countdown_mutex;
    uint32_t m_countdown_counter;
};


#endif
