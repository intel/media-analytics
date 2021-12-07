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

#include "ConnectorDispatch.h"
#include <unistd.h>

#include <chrono>
using namespace std::chrono;

VAConnectorDispatch::VAConnectorDispatch(uint32_t maxInput, uint32_t maxOutput, uint32_t bufferNum):
    VAConnector(maxInput, maxOutput)
{
    uint32_t totalBufferNum = maxOutput * bufferNum;
    m_packets.resize(totalBufferNum);
    for (int i = 0; i < totalBufferNum; i++)
    {
        m_inPipe.push_back(&m_packets[i]);
    }

    std::vector<std::mutex> mlist(maxOutput);
    std::vector<std::condition_variable> clist(maxOutput);

    m_outMutex.swap(mlist);
    m_conds.swap(clist);

    m_outPipes.resize(maxOutput);
}

VAConnectorDispatch::~VAConnectorDispatch()
{
}

VADataPacket *VAConnectorDispatch::GetInput(int index)
{
    VADataPacket *buffer = NULL;
    do
    {
        std::unique_lock<std::mutex> lock(m_inMutex);
        if (m_inPipe.size() > 0)
        {
            buffer = m_inPipe.front();
            m_inPipe.pop_front();
        }
        else
        {
            lock.unlock();
            usleep(500);
        }
    } while (buffer == NULL);
    return buffer;
}

void VAConnectorDispatch::StoreInput(int index, VADataPacket *data)
{
    uint32_t channel = data->front()->ChannelIndex();
    uint32_t outIndex = channel%m_maxOut;
    {
        std::lock_guard<std::mutex> lock(m_outMutex[outIndex]);
        m_outPipes[outIndex].push_back(data);
    }
    m_conds[outIndex].notify_one();
}

void VAConnectorDispatch::Trigger()
{
}

VADataPacket *VAConnectorDispatch::GetOutput(int index, const timespec *abstime, const VAConnectorPin*) 
{
    bool timeout = false;
    VADataPacket *buffer = nullptr;
    std::unique_lock<std::mutex> lock(m_outMutex[index]);
    do
    {
        if (m_outPipes[index].size() > 0)
        {
            buffer = m_outPipes[index].front();
            
            m_outPipes[index].pop_front();
        }
        else if (abstime == nullptr)
        {
            m_conds[index].wait(lock);
        }
        else if (!timeout)
        {
            m_conds[index].wait_until(lock, timespec2tp(*abstime));
            timeout = true;
        }
        else if (timeout)
        {
            break;
        }
    } while(buffer == NULL);
    return buffer;
}

void VAConnectorDispatch::StoreOutput(int index, VADataPacket *data)
{
    std::lock_guard<std::mutex> lock(m_inMutex);
    m_inPipe.push_front(data);
}

