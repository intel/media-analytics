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

#ifndef __CONNECTOR_H__
#define __CONNECTOR_H__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <list>
#include <mutex>
#include <stdexcept>
#include "logs.h"
#include "DataPacket.h"

class VAConnectorPin;
class VAThreadBlock;

class VAConnector
{
friend class VAConnectorPin;
public:
    VAConnector(uint32_t maxInput, uint32_t maxOutput);
    virtual ~VAConnector();

    VAConnector(const VAConnector&) = delete;
    VAConnector& operator=(const VAConnector&) = delete;

    VAConnectorPin *NewInputPin();
    VAConnectorPin *NewOutputPin();

    void DisconnectPin(VAConnectorPin* pin, bool isInput);
    
protected:
    virtual VADataPacket *GetInput(int index) = 0;
    virtual void StoreInput(int index, VADataPacket *data) = 0;
    virtual VADataPacket *GetOutput(int index, const timespec *abstime = nullptr,
        const VAConnectorPin *calledPin = nullptr) = 0;
    virtual void StoreOutput(int index, VADataPacket *data) = 0;
    virtual void Trigger() = 0;
    std::list<VAConnectorPin *> m_inputPins;
    std::list<VAConnectorPin *> m_outputPins;
    std::list<VAConnectorPin *> m_inputDisconnectedPins;
    std::list<VAConnectorPin *> m_outputDisconnectedPins;

    uint32_t m_maxIn;
    uint32_t m_maxOut;

    std::mutex m_InputMutex;
    std::mutex m_OutputMutex;

    bool m_noInput;
    bool m_noOutput;
};

class VAConnectorPin
{
public:
    VAConnectorPin(VAConnector *connector, int index, bool isInput):
        m_connector(connector),
        m_index(index),
        m_isInput(isInput)
    {
        printf("pin index %d\n", m_index);
    }

    virtual ~VAConnectorPin() {}

    VAConnectorPin(const VAConnectorPin&) = delete;
    VAConnectorPin& operator=(const VAConnectorPin&) = delete;
    
    virtual VADataPacket *Get()
    {
        if (m_isInput)
            return m_connector->GetInput(m_index);
        else
            return m_connector->GetOutput(m_index, nullptr, this);
    }
    
    virtual void Store(VADataPacket *data)
    {
        if (m_isInput)
            m_connector->StoreInput(m_index, data);
        else
            m_connector->StoreOutput(m_index, data);
    }

    void Disconnect()
    {
        if (m_connector != nullptr)
            m_connector->DisconnectPin(this, m_isInput);
    }

protected:
    VAConnector *m_connector;
    const int m_index;
    bool m_isInput;
};

class VASinkPin : public VAConnectorPin
{
public:
    VASinkPin():
        VAConnectorPin(nullptr, 0, true) {}

    VASinkPin(const VASinkPin&) = delete;
    VASinkPin& operator=(const VASinkPin&) = delete;

    VADataPacket *Get()
    {
        return &m_packet;
    }

    void Store(VADataPacket *data)
    {
        for (auto ite = data->begin(); ite != data->end(); ite ++)
        {
            VAData *data = *ite;
            //printf("Warning: in sink, still referenced data: %d, channel %d, frame %d\n", data->Type(), data->ChannelIndex(), data->FrameIndex());
            data->SetRef(0);
            VADataCleaner::getInstance().Add(data);
        }
        data->clear();
    }
protected:
    VADataPacket m_packet;
};

class VAFilePin: public VAConnectorPin
{
public:
    VAFilePin(const char *filename, bool endless = true):
        VAConnectorPin(nullptr, 0, false),
        m_fp(nullptr),
        m_endless(endless)
    {
        INFO("VAFilePin open filename %s", filename);
        m_fp = fopen(filename, "rb");
        if (m_fp)
        {
            INFO("%s  file opened successfully", filename);
            fseek(m_fp, 0, SEEK_SET);
            m_dataSize = 1024 * 1024;
            m_data = (uint8_t *)malloc(m_dataSize);
            if (!m_data)
            {
                ERRLOG("Not enough free memory for m_data");
                fclose(m_fp);
                m_fp = nullptr;
                throw std::runtime_error("Not enough free memory");
            }
        }
        else
        {
            ERRLOG("%s  file open failed", filename);
            throw std::runtime_error("File open failed");
        }
    }

    VAFilePin(const VAFilePin&) = delete;
    VAFilePin& operator=(const VAFilePin&) = delete;

    ~VAFilePin()
    {
        free(m_data);
        fclose(m_fp);
    }

    VADataPacket *Get()
    {
        uint32_t num = fread(m_data, 1, m_dataSize, m_fp);
        if (num == 0 && m_endless)
        {
            fseek(m_fp, 0, SEEK_SET);
            //num = fread(m_data, 1, m_dataSize, m_fp);
            // a VAData with size 0 means the stream is to the end
        }

        VAData *data = VAData::Create(m_data, 0, num);
        m_packet.push_back(data);
        return &m_packet;
    }

    void Store(VADataPacket *data)
    {
        if (data != &m_packet)
        {
            printf("Error: store a wrong packet in VAFilePin\n");
        }
    }

protected:
    FILE *m_fp;
    uint8_t *m_data;
    uint32_t m_dataSize;
    bool m_endless;

    VADataPacket m_packet;
};

class VARawFilePin: public VAConnectorPin
{
public:
    VARawFilePin(const char *filename, uint32_t fourcc, uint32_t width, uint32_t height, bool endless = true);

    ~VARawFilePin()
    {
        free(m_data);
        fclose(m_fp);
    }

    VARawFilePin(const VARawFilePin&) = delete;
    VARawFilePin& operator=(const VARawFilePin&) = delete;

    VADataPacket *Get()
    {
        uint32_t num = fread(m_data, 1, m_dataSize, m_fp);
        if (num == 0 && m_endless)
        {
            fseek(m_fp, 0, SEEK_SET);
            //num = fread(m_data, 1, m_dataSize, m_fp);
            // a VAData with size 0 means the stream is to the end
        }

        VAData *data = VAData::Create(m_data, m_width, m_height, m_width, m_fourcc);
        m_packet.push_back(data);
        return &m_packet;
    }

    void Store(VADataPacket *data)
    {
        if (data != &m_packet)
        {
            printf("Error: store a wrong packet in VAFilePin\n");
        }
    }

protected:
    FILE *m_fp;
    uint8_t *m_data;
    uint32_t m_dataSize;
    bool m_endless;

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_fourcc;

    VADataPacket m_packet;
};


class VACsvWriterPin : public VAConnectorPin
{
public:
    VACsvWriterPin(const char *filename):
        VAConnectorPin(nullptr, 0, true),
        m_fp(nullptr)
    {
        INFO("VACsvWriterPin open filename %s", filename);
        m_fp = fopen(filename, "w");
        if (m_fp)
        {
            INFO("%s  file created successful", filename);
        }
        else
        {
            ERRLOG("%s  file creation failed", filename);
            throw std::runtime_error("File creation failed");
        }
    }

    ~VACsvWriterPin()
    {
        if (m_fp)
        {
            fclose(m_fp);
        }
    }

    VACsvWriterPin(const VACsvWriterPin&) = delete;
    VACsvWriterPin& operator=(const VACsvWriterPin&) = delete;

    VADataPacket *Get()
    {
        return &m_packet;
    }

    void Store(VADataPacket *data);

protected:
    VADataPacket m_packet;
    FILE *m_fp;
};

static inline std::chrono::nanoseconds timespec2dur(const timespec& ts)
{
    auto dur = std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec};
    return std::chrono::duration_cast<std::chrono::nanoseconds>(dur);
}

static inline std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> timespec2tp(const timespec& ts)
{
    return std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(timespec2dur(ts)) };
}

#endif
