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

#include "Statistics.h"
#include <unistd.h>
#include <signal.h>
#include <math.h>

static bool _continue = true;
// handle to end the process
void sigint_hanlder(int s)
{
    _continue = false;
}

Statistics::Statistics()
{
    std::vector<std::mutex> mlist(STATISTICS_TYPE_NUM);
    m_mutex.swap(mlist);
    for (int i = 0; i < STATISTICS_TYPE_NUM; i++)
    {
        m_counters[i] = 0;
        m_countersCurrentCycle[i] = 0;
        m_accCounters[i] = 0;
        m_accNum[i] = 0;
    }
    // set the handler of ctrl-c
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = sigint_hanlder;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    m_countdown_counter = 0;
}

Statistics::~Statistics()
{
}

void Statistics::Step(StatisticsType type)
{
    std::lock_guard<std::mutex> lock(m_mutex[type]);
    ++ m_counters[type];
}

void Statistics::CountDown()
{
    std::lock_guard<std::mutex> lock(m_countdown_mutex);
    -- m_countdown_counter;
    if (m_countdown_counter == 0)
    {
        _continue = false;
    }
}

void Statistics::Update(StatisticsType type)
{

    if (type == STATISTICS_TYPE_NUM)
    {
        for (int i = 0; i < STATISTICS_TYPE_NUM; i++)
        {
            std::lock_guard<std::mutex> lock(m_mutex[i]);
            m_accCounters[i] += m_counters[i];
            m_accNum[i] ++;
            m_countersCurrentCycle[i] = m_counters[i];
            m_counters[i] = 0;
        }
    }
    else
    {
        std::lock_guard<std::mutex> lock(m_mutex[type]);
        m_accCounters[type] += m_counters[type];
        m_accNum[type] ++;
        m_countersCurrentCycle[type] = m_counters[type];
        m_counters[type] = 0;
    }
}

void Statistics::Report()
{
    printf("%7d | %7d | %7ld | %7d | %7ld | %10ld | %10ld | %10ld | %10ld | %10ld\n",
        m_accNum[DECODED_FRAMES],
        m_countersCurrentCycle[DECODED_FRAMES],
        m_accCounters[DECODED_FRAMES] / m_accNum[DECODED_FRAMES],
        m_countersCurrentCycle[INFERENCE_FRAMES_PROCESSED],
        m_accCounters[INFERENCE_FRAMES_PROCESSED] / m_accNum[INFERENCE_FRAMES_PROCESSED],
        m_accCounters[DECODED_FRAMES],
        m_accCounters[INFERENCE_FRAMES_OD_RECEIVED],
        m_accCounters[INFERENCE_FRAMES_OD_PROCESSED],
        m_accCounters[INFERENCE_FRAMES_OC_RECEIVED],
        m_accCounters[INFERENCE_FRAMES_OC_PROCESSED]);
}

bool Statistics::IsStarted()
{
    // This function checkes if Decode/OD/OC has processed any frame.
    bool bHasDecodedAFrame = m_accCounters[DECODED_FRAMES] > 0;
    bool bHasProcessedAnODFrame = (m_accCounters[INFERENCE_FRAMES_OD_RECEIVED] > 0) ?
        (m_accCounters[INFERENCE_FRAMES_OD_PROCESSED] > 0) : true;
    bool bHasProcessedAnOCFrame = (m_accCounters[INFERENCE_FRAMES_OC_RECEIVED] > 0) ?
        (m_accCounters[INFERENCE_FRAMES_OC_PROCESSED] > 0) : true;
    return bHasDecodedAFrame && bHasProcessedAnODFrame && bHasProcessedAnOCFrame;
}

void Statistics::ReportPeriodly(float period, int duration)
{
    int count = 0;
    int countNoFPS = 0;
    bool endless = (duration < 0);
    //below header assume "period" is 1.0. ie. report statistics in 1.0 second interval.
    //currFPS            = fps for this second for decode/inference
    //avgFPS             = average fps up to this second for decode/inference
    //DEC Out TotalFrame = cumulative decoded frames
    //OD  In  TotalFrame = cumulative # of frames submitted to OD
    //OD  Out TotalFrame = cumulative # of frames processed by OD
    //OC  In  TotalFrame = cumulative # of frames submitted to OC
    //OC  Out TotalFrame = cumulative # of frames processed by OC
    printf("----------------------------------------------------------------------------------------------------------------\n");
    printf("Elapsed | Decode            | Inference         | DEC Out    | OD In      | OD Out     | OC In      | OC Out\n");
    printf("Time(s) | currFPS | avgFPS  | currFPS | avgFPS  | TotalFrame | TotalFrame | TotalFrame | TotalFrame | TotalFrame\n");
    printf("----------------------------------------------------------------------------------------------------------------\n");

    while (_continue)
    {
        uint32_t time = (uint32_t)(period * 1000000);
        usleep(time);
        Update();
        Report();
        if (endless)
        {
            //start counting after engine started to avoid startup time exit
            if (IsNoFPS() && IsStarted())
            {
                printf("warning: no output from decode and inference for 1 second\n");
                if (++countNoFPS == 5)
                {
                    printf("warning: no output from decode and inference far too long, aborting...\n");
                    break;
                }
            }
            else
                countNoFPS = 0;
        }
        ++ count;
        if (!endless && count >= (int)ceil(duration / period))
        {
            break;
        }
    }
}
