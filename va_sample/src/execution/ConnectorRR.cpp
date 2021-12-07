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

#include "ConnectorRR.h"
#include <unistd.h>
#include <algorithm>

VAConnectorRR::VAConnectorRR(uint32_t maxInput, uint32_t maxOutput, uint32_t bufferNum):
    VAConnector(maxInput, maxOutput)
{
    uint32_t totalBufferNum = maxInput * bufferNum;
    m_packets = new VADataPacket[totalBufferNum];
    for (int i = 0; i < totalBufferNum; i++)
    {
        m_inPipe.push_back(&m_packets[i]);
    }
}

VAConnectorRR::~VAConnectorRR()
{
    if (m_packets)
    {
        delete[] m_packets;
    }
}

VADataPacket *VAConnectorRR::GetInput(int index)
{
    VADataPacket *buffer = NULL;
    do
    {
        if (m_noInput || m_noOutput)
        {
            Trigger();
            break;
        }

        std::unique_lock<std::mutex> lock(m_mutex);
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

void VAConnectorRR::StoreInput(int index, VADataPacket *data)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outPipe.push_back(data);
    }
    m_cond.notify_one();
}

void VAConnectorRR::Trigger()
{
    m_cond.notify_all();
}

VADataPacket *VAConnectorRR::GetOutput(int index, const timespec *abstime,
                                       const VAConnectorPin *calledPin) 
{
    bool timeout = false;
    VADataPacket *buffer = nullptr;

    std::unique_lock<std::mutex> lock(m_mutex);
    do
    {
        if (m_noInput || m_noOutput)
        {
            Trigger();
            break;
        }

        if (calledPin)
        {
            std::list<VAConnectorPin *>::iterator iter =
                std::find(m_outputDisconnectedPins.begin(),
                          m_outputDisconnectedPins.end(), calledPin);
            if (iter != m_outputDisconnectedPins.end()) // pin has been disconnected
                break;
        }

        if (m_outPipe.size() > 0)
        {
            buffer = m_outPipe.front();
            m_outPipe.pop_front();
        }
        else if (abstime == nullptr)
        {
            m_cond.wait(lock);
        }
        else if (!timeout)
        {
            m_cond.wait_until(lock, timespec2tp(*abstime));
            timeout = true;
        }
        else if (timeout)
        {
            break;
        }
    } while(buffer == NULL);
    return buffer;
}

void VAConnectorRR::StoreOutput(int index, VADataPacket *data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inPipe.push_front(data);
}

