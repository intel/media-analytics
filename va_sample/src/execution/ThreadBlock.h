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

#ifndef __THREAD_BLOCK_H__
#define __THREAD_BLOCK_H__
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <vector>

#include "DataPacket.h"
#include "Connector.h"

class VAThreadBlock
{
public:
    VAThreadBlock();
    virtual ~VAThreadBlock();

    static void RunAllThreads();

    static void StopAllThreads();

    virtual int Run();
    virtual void Stop();
    virtual void Join();
    virtual int Prepare();
    virtual int Loop() = 0;

    inline void ConnectInput(VAConnectorPin *pin) {m_inputPin = pin; }
    inline void ConnectOutput(VAConnectorPin *pin) {m_outputPin = pin; }

    inline void Finish() {m_finish = true; }

protected:
    VADataPacket* AcquireInput()
    {
        if (!m_inputPin)
            return nullptr;

        VADataPacket* packet = m_inputPin->Get();
        return packet;
    }
    
    void ReleaseInput(VADataPacket* data)
    {
        if (!m_inputPin)
            return;

        data->clear();
        m_inputPin->Store(data);
    }
    
    VADataPacket* DequeueOutput()
    {
        if (!m_outputPin)
            return nullptr;

        VADataPacket *packet = m_outputPin->Get();
        if (!packet || !packet->empty())
            return nullptr;

        return packet;
    }
    
    void EnqueueOutput(VADataPacket *data)
    {
        if (!m_outputPin)
            return;

        m_outputPin->Store(data);
    }

    void DisconnectPin()
    {
        if (m_inputPin)
        {
            m_inputPin->Disconnect();
            m_inputPin = nullptr;
        }

        if (m_outputPin)
        {
            m_outputPin->Disconnect();
            m_outputPin = nullptr;
        }
    }

    virtual int PrepareInternal() { return 0; }
    
    VAConnectorPin *m_inputPin;
    VAConnectorPin *m_outputPin;

    pthread_t m_threadId;

    static std::vector<VAThreadBlock *> m_allThreads;

    bool m_stop;
    bool m_finish;
};

#endif
