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

#include "InferenceMobileSSD.h"
#include <ie_plugin_config.hpp>
#include <inference_engine.hpp>
#include <logs.h>
#include "DataPacket.h"

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

InferenceMobileSSD::InferenceMobileSSD():
    m_inputWidth(0),
    m_inputHeight(0),
    m_channelNum(1),
    m_resultSize(0),
    m_maxResultNum(0)
{
}

InferenceMobileSSD::~InferenceMobileSSD()
{
}

void InferenceMobileSSD::SetDataPorts()
{
    TRACE("");
    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    if (inputInfo.begin() == inputInfo.end())
    {
        ERRLOG("inputInfo is empty!\n");
        return;
    }

    auto& inputInfoFirst = inputInfo.begin()->second;
    inputInfoFirst->setPrecision(Precision::U8);
    inputInfoFirst->getInputData()->setLayout(Layout::NCHW);
    if (m_shareSurfaceWithVA)
    {
        inputInfoFirst->getPreProcess().setColorFormat(ColorFormat::NV12);
    }

    // ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	_output->setPrecision(Precision::FP32);
	_output->setLayout(Layout::NCHW);	
}

int InferenceMobileSSD::Load(const char *device, const char *model, const char *weights)
{
    int ret = InferenceOV::Load(device, model, weights);
    INFO("device %s, model %s, weights %s\n", device, model, weights);
    if (ret)
    {
        ERRLOG(" failed  ret %d \n", ret);
        return ret;
    }

    // ---------------------------Set outputs ------------------------------------------------------	
    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    auto& _output = outputInfo.begin()->second;
    const InferenceEngine::SizeVector outputDims = _output->getTensorDesc().getDims();
    m_maxResultNum= outputDims[2];
    m_resultSize = outputDims[3];

    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    auto& inputInfoFirst = inputInfo.begin()->second;
    const InferenceEngine::SizeVector inputDims = inputInfoFirst->getTensorDesc().getDims();
    m_channelNum = inputDims[1];
    m_inputWidth = inputDims[3];
    m_inputHeight = inputDims[2];

    return 0;
}

void InferenceMobileSSD::GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc)
{
    TRACE("");
    *width = m_inputWidth;
    *height = m_inputHeight;
    *fourcc = m_shareSurfaceWithVA?0x3231564e:0x50424752; //MFX_FOURCC_RGBP
}

void InferenceMobileSSD::CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex)
{
    uint8_t *input = (uint8_t *)dst;
    input += batchIndex * m_inputWidth * m_inputHeight * m_channelNum;
    //
    // The img in BGR format
    //cv::Mat imgMat(m_inputHeight, m_inputWidth, CV_8UC1, img);
    //std::vector<cv::Mat> inputChannels;
    //for (int i = 0; i < m_channelNum; i ++)
    //{
    //    cv::Mat channel(m_inputHeight, m_inputWidth, CV_8UC1, input + i * m_inputWidth * m_inputHeight);
    //    inputChannels.push_back(channel);
    //}
    //cv::split(imgMat, inputChannels);

    //
    // The img should already in RGBP format, if not, using the above code
    memcpy(input, img, m_channelNum * m_inputWidth * m_inputHeight);
    TRACE("");
}

int InferenceMobileSSD::Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds, uint32_t *roiIds)
{
    TRACE("");
    std::map<std::string, const float*>* curResults = (std::map<std::string, const float*>*) result;
    float* curResult = (float*)curResults->find(m_outputsNames[0])->second;
    typedef std::vector<VAData *> VADataVector;
    VADataVector *tempBuffer = new VADataVector[count];

    for (int i = 0; i < m_maxResultNum; i ++)
    {
        int imgid = (int)curResult[0];
        if (imgid < 0 || curResult[2] == 0 || imgid > count)
        {
            break;
        }
        int c = (int)curResult[1];
        float conf = curResult[2];
        if(curResult[2] < m_confidenceThreshold){
            curResult += m_resultSize;
            continue;
        }
        float l = curResult[3];
        float t = curResult[4];
        float r = curResult[5];
        float b = curResult[6];
        if (l < 0.0) l = 0.0;
        if (t < 0.0) t = 0.0;
        if (r > 1.0) r = 1.0;
        if (b > 1.0) b = 1.0;
        VAData *data = VAData::Create(l, t, r, b, c, conf);
        data->SetID(channelIds[imgid], frameIds[imgid]);
        // ssd model may create multip roi regions for one frame, re-index the rois
        data->SetRoiIndex(tempBuffer[imgid].size());
        tempBuffer[imgid].push_back(data);
        curResult += m_resultSize;
    }

    for (int i = 0; i < count; i++)
    {
        datas.insert(datas.end(), tempBuffer[i].begin(), tempBuffer[i].end());
    }

    delete[] tempBuffer;
    return 0;
}

