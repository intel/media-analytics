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

#include "Connector.h"
#include <string.h>
#include <algorithm>
#include <vector>

VAConnector::VAConnector(uint32_t maxInput, uint32_t maxOutput):
    m_maxIn(maxInput),
    m_maxOut(maxOutput),
    m_noInput(true),
    m_noOutput(true)
{
}

VAConnector::~VAConnector()
{
    // delete all pins
    while (!m_inputPins.empty())
    {
        VAConnectorPin *pin = m_inputPins.back();
        delete pin;
        m_inputPins.pop_back();
    }
    while (!m_outputPins.empty())
    {
        VAConnectorPin *pin = m_outputPins.back();
        delete pin;
        m_outputPins.pop_back();
    }
    while (!m_inputDisconnectedPins.empty())
    {
        VAConnectorPin *pin = m_inputDisconnectedPins.back();
        delete pin;
        m_inputDisconnectedPins.pop_back();
    }
    while (!m_outputDisconnectedPins.empty())
    {
        VAConnectorPin *pin = m_outputDisconnectedPins.back();
        delete pin;
        m_outputDisconnectedPins.pop_back();
    }
}

VAConnectorPin *VAConnector::NewInputPin()
{
    std::lock_guard<std::mutex> lock(m_InputMutex);
    VAConnectorPin *pin = nullptr;
    if (m_inputPins.size() < m_maxIn)
    {
        pin = new VAConnectorPin(this, m_inputPins.size(), true);
        m_inputPins.push_back(pin);
        m_noInput = false;
    }
    else
    {
        printf("Can't allocate more input pins\n");
    }
    return pin;
}

VAConnectorPin *VAConnector::NewOutputPin()
{
    VAConnectorPin *pin = nullptr;
    std::lock_guard<std::mutex> lock(m_OutputMutex);
    if (m_outputPins.size() < m_maxOut)
    {
        pin = new VAConnectorPin(this, m_outputPins.size(), false);
        m_outputPins.push_back(pin);
        m_noOutput = false;
    }
    else
    {
        printf("Can't allocate more output pins\n");
    }
    return pin;
}

void VAConnector::DisconnectPin(VAConnectorPin* pin, bool isInput)
{
    if (isInput)
    {
        std::lock_guard<std::mutex> lock(m_InputMutex);
        std::list<VAConnectorPin *>::iterator iter =
            std::find(m_inputPins.begin(), m_inputPins.end(), pin);
        if (iter != m_inputPins.end())
        {
            m_inputDisconnectedPins.push_back(*iter);
            m_inputPins.erase(iter);
            Trigger();
        }

        if (m_inputPins.size() == 0)
        {
            m_noInput = true;
            Trigger();
        }
    }
    else
    {
        std::lock_guard<std::mutex> lock(m_OutputMutex);
        std::list<VAConnectorPin *>::iterator iter =
            std::find(m_outputPins.begin(), m_outputPins.end(), pin);
        if (iter != m_outputPins.end())
        {
            m_outputDisconnectedPins.push_back(*iter);
            m_outputPins.erase(iter);
            Trigger();
        }

        if (m_outputPins.size() == 0)
        {
            m_noOutput = true;
            Trigger();
        }
    }
}

void VACsvWriterPin::Store(VADataPacket *data)
{
    int size = data->size();

    std::vector<int> classes(size, 0);
    std::vector<float> confs(size, 0);
    std::vector<float> lefts(size, 0);
    std::vector<float> tops(size, 0);
    std::vector<float> rights(size, 0);
    std::vector<float> bottoms(size, 0);

    uint32_t roiNum = 0;
    uint32_t channel = 0;
    uint32_t frame = 0;
    bool hasDump = false;
    for (auto ite = data->begin(); ite != data->end(); ite ++)
    {
        VAData *data = *ite;
        channel = data->ChannelIndex();
        frame = data->FrameIndex();

        if (data->Type() == ROI_REGION)
        {
            float left, top, right, bottom;
            data->GetRoiRegion(&left, &top, &right, &bottom);
            uint32_t index = data->RoiIndex();
            if (index > roiNum)
            {
                roiNum = index;
            }
            lefts[index] = left;
            tops[index] = top;
            rights[index] = right;
            bottoms[index] = bottom;
            classes[index] = data->Class();
            confs[index] = data->Confidence();
            hasDump = true;
        }
        else if (data->Type() == IMAGENET_CLASS)
        {
            uint32_t index = data->RoiIndex();
            if (index > roiNum)
            {
                roiNum = index;
            }
            classes[index] = data->Class();
            confs[index] = data->Confidence();
            hasDump = true;
        }
        data->SetRef(0);
        VADataCleaner::getInstance().Add(data);
    }
    data->clear();
    ++ roiNum;
    for (int i = 0; i < roiNum && hasDump; i++)
    {
        fprintf(m_fp, "%d, %d, %d, %f, %f, %f, %f, %d, %f\n", channel,
                                                              frame,
                                                              i,
                                                              lefts[i],
                                                              tops[i],
                                                              rights[i],
                                                              bottoms[i],
                                                              classes[i],
                                                              confs[i]);
    }
    fflush(m_fp);
}

VARawFilePin::VARawFilePin(const char *filename, uint32_t fourcc, uint32_t width, uint32_t height, bool endless):
        VAConnectorPin(nullptr, 0, false),
        m_fp(nullptr),
        m_fourcc(fourcc),
        m_width(width),
        m_height(height),
        m_endless(endless)
    {
        m_fp = fopen(filename, "rb");
        if (!m_fp)
            throw std::bad_alloc();

        fseek(m_fp, 0, SEEK_SET);
        if (fourcc == MFX_FOURCC_NV12)
        {
            m_dataSize = m_width * m_height * 3 / 2;
        }
        else
        {
            m_dataSize = m_width * m_height * 3;
        }
        m_data = (uint8_t *)malloc(m_dataSize);
        if (!m_data) {
            fclose(m_fp);
            throw std::bad_alloc();
        }
    }
