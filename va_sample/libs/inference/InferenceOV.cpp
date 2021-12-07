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

#include "InferenceOV.h"
#include <logs.h>
#include <cldnn/cldnn_config.hpp>

#include <ie_plugin_config.hpp>
#include <inference_engine.hpp>

#include <ie_compound_blob.h>

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

InferenceOV::InferenceOV():
    m_asyncDepth(1),
    m_batchNum(1),
    m_batchIndex(0),
    m_modelInputReshapeWidth(0),
    m_modelInputReshapeHeight(0),
    m_vaDisplay(nullptr),
    m_shareSurfaceWithVA(false),
    m_confidenceThreshold(0.8)
{
}

InferenceOV::~InferenceOV()
{
    while (m_busyRequest.size() != 0)
    {
        Wait();
        m_busyRequest.pop();
    }
    while (m_freeRequest.size() != 0)
    {
        m_freeRequest.pop();
    }
    m_batchedBlobs.clear();
}

int InferenceOV::Initialize(uint32_t batch_num, uint32_t async_depth, float confidence_threshold,
                        uint32_t model_input_reshape_height, uint32_t model_input_reshape_width)
{
    m_batchNum = batch_num;
    m_asyncDepth = async_depth;
    m_confidenceThreshold = confidence_threshold;
    m_modelInputReshapeHeight = model_input_reshape_height;
    m_modelInputReshapeWidth = model_input_reshape_width;
    TRACE(" m_batchNum %d m_asyncDepth %d m_confidenceThreshold %d m_modelInputReshapeHeight %d m_modelInputReshapeWidth %d\n",
        m_batchNum, m_asyncDepth, m_confidenceThreshold, m_modelInputReshapeHeight, m_modelInputReshapeWidth);
    return 0;
}


void InferenceOV::IECoreInfo(const char* d)
{
    std::map<std::string, Version> vm =  m_ie.GetVersions(d);
    for(auto p : vm) {
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        int x = p.second.apiVersion.major;
        int y = p.second.apiVersion.minor;
#pragma GCC diagnostic pop
        string bn = p.second.buildNumber;
        string desc = p.second.description;
        string vs = std::to_string(x) + "." + std::to_string(y);
        cout << "INFO: device: " << p.first << ", version: " << vs << endl;
        cout << "INFO: build number: " << bn << endl;
        cout << "INFO: description: " << desc << endl;
    }
}

int InferenceOV::Load(const char *device, const char *model, const char *weights)
{
    m_network = m_ie.ReadNetwork(model);
    m_network.setBatchSize(m_batchNum);
    m_batchNum = m_network.getBatchSize();

    INFO("Batch number get from network is %d\n", m_batchNum);
    // ---------------------------reshape model height, width if requested--------------------------
    auto input_shapes = m_network.getInputShapes();
    if (input_shapes.empty())
    {
        ERRLOG("input_shapes is empty");
        return -1;
    }
    InferenceEngine::SizeVector input_shape;
    std::tie(m_inputName, input_shape) = *input_shapes.begin();
    if (input_shape.empty())
    {
        ERRLOG("input_shape is empty");
        return -1;
    }
    if ((m_modelInputReshapeHeight != 0 && m_modelInputReshapeWidth != 0) &&
        !((input_shape[2] == m_modelInputReshapeHeight) && (input_shape[3] == m_modelInputReshapeWidth)))
    {
        //default layout of input_shape: NCHW for 4-dimensional
        input_shape[2] = m_modelInputReshapeHeight;//height
        input_shape[3] = m_modelInputReshapeWidth;//width
        input_shapes[m_inputName] = input_shape;
        m_network.reshape(input_shapes);
    }
    else
    {
        m_modelInputReshapeHeight = input_shape[2];//height
        m_modelInputReshapeWidth = input_shape[3];//width
    }

    IECoreInfo(device);

    SetDataPorts();
    TRACE("SetDataPorts done");
    // ---------------------------Set inputs ------------------------------------------------------	
	InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
	m_inputName = inputInfo.begin()->first;
    TRACE("inputInfo  %s",  m_inputName.c_str());

    // ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    for (auto& output : outputInfo) {
        m_outputsNames.push_back(output.first);
        TRACE("outputInfo %s ", output.first.c_str());
    }

    if (m_outputsNames.size() < 1)
    {
        ERRLOG("m_outputsNames.size() < 1. Expects at least 1.\n");
        return -1;
    }

    // -------------------------Loading model to the plugin-------------------------------------------------
    //TRACE("---- Loading model to the plugin");
	//InferenceEngine::ExecutableNetwork exenet;
    try {
        if (m_vaDisplay == nullptr || m_shareSurfaceWithVA == false)
        {   
            m_execNetwork= m_ie.LoadNetwork(m_network, device, {});
        }
        else
        {
            // currently only support nv12 if surface sharing with libva
            m_sharedContext = gpu::make_shared_context(m_ie, device, m_vaDisplay);
            m_execNetwork = m_ie.LoadNetwork(m_network, m_sharedContext,
                {{ GPUConfigParams::KEY_GPU_NV12_TWO_INPUTS, PluginConfigParams::YES } });
        }
    }
    catch (InferenceEngine::Exception e) {
        ERRLOG("Input Model file %s is not supported by current device: %s",  model, device);
        std::cout<<"   Input Model file"<< model<<" is not supported by current device:"<<device<<std::endl;
        return -1;
    }

    for (int i = 0; i < m_asyncDepth; i++)
    {
        InferRequest::Ptr request = std::make_shared<InferRequest>(m_execNetwork.CreateInferRequest());
        m_freeRequest.push(request);
    }

    return 0;
}

int InferenceOV::InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId, uint32_t roiId)
{
    TRACE("img %p, channelId %d  frameId %d, roiId %d  \n", img, channelId, frameId, roiId);

    if (m_freeRequest.size() == 0)
    {
        //std::cout << "Warning: No free worker in Inference now" << std::endl;
        Wait();
        GetOutputInternal(m_internalDatas, m_internalChannels, m_internalFrames);
    }

    InferRequest::Ptr curRequest = m_freeRequest.front();
    void *dst = curRequest->GetBlob(m_inputName)->buffer();

    CopyImage(img, dst, m_batchIndex);

    m_channels.push(channelId);
    m_frames.push(frameId);
    m_rois.push(roiId);

    ++ m_batchIndex;
    if (m_batchIndex >= m_batchNum)
    {
        m_batchIndex = 0;
        curRequest->StartAsync();
        m_busyRequest.push(curRequest);
        m_freeRequest.pop();
    }

    return 0;
}

static stopWatch watch;


int InferenceOV::InsertImage(const int surfID, uint32_t channelId, uint32_t frameId, uint32_t roiId)
{
    TRACE(" surfID %d, channelId %d  frameId %d, roiId %d  \n", surfID, channelId, frameId, roiId);

    if (!m_shareSurfaceWithVA)
    {
        ERRLOG("VASurface sharing not enabled\n");
        return -1;
    }
    if (m_freeRequest.size() == 0)
    {
        //std::cout << "Warning: No free worker in Inference now" << std::endl;
        Wait();
        GetOutputInternal(m_internalDatas, m_internalChannels, m_internalFrames);
    }

    InferRequest::Ptr curRequest = m_freeRequest.front();
    auto nv12_blob = gpu::make_shared_blob_nv12(GetInputHeight(), GetInputWidth(), m_sharedContext, surfID);
    m_batchedBlobs.push_back(nv12_blob);

    m_channels.push(channelId);
    m_frames.push(frameId);
    m_rois.push(roiId);

    ++ m_batchIndex;
    if (m_batchIndex >= m_batchNum)
    {
        m_batchIndex = 0;
        //startTimer(&watch);
        auto blobs = make_shared_blob<BatchedBlob>(m_batchedBlobs);
        curRequest->SetBlob(m_inputName, blobs);
        TRACE("StartAsync inference");
        //stopTimer(&watch);
        //printf("In inferenceOV, SetBlob takes %fms\n", watch.elapsed);
        //startTimer(&watch);
        curRequest->StartAsync();
        //stopTimer(&watch);
        //printf("In inferenceOV, StartAsync takes %fms\n", watch.elapsed);
        m_busyRequest.push(curRequest);
        m_freeRequest.pop();
        m_batchedBlobs.clear();
    }
    return 0;
}

int InferenceOV::Wait()
{
    if (m_busyRequest.size() == 0)
    {
        return -1;
    }
    TRACE("");
    InferRequest::Ptr curRequest = m_busyRequest.front();
    InferenceEngine::StatusCode ret = curRequest->Wait(IInferRequest::WaitMode::RESULT_READY);
    TRACE("InferenceEngine::StatusCode %X", (int)ret);
    if (ret == InferenceEngine::OK)
    {
        return 0;
    }
    else
    {
        ERRLOG("return value %Xx \n", (int)ret);
        return ret;
    }
}

int InferenceOV::GetOutput(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames)
{
    TRACE("");
    if (m_internalDatas.size() > 0 && m_internalChannels.size() > 0 && m_internalFrames.size() > 0)
    {
        datas.insert(datas.end(), m_internalDatas.begin(), m_internalDatas.end());
        m_internalDatas.clear();
        channels.insert(channels.end(), m_internalChannels.begin(), m_internalChannels.end());
        m_internalChannels.clear();
        frames.insert(frames.end(), m_internalFrames.begin(), m_internalFrames.end());
        m_internalFrames.clear();
    }
    return GetOutputInternal(datas, channels, frames);
}

int InferenceOV::GetOutputInternal(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames)
{

    if (m_busyRequest.size() == 0 && m_batchIndex == 0)
    {
        // inference free, can pop all the results, we need the input
        return -1;
    }
    if (m_busyRequest.size() == 0 && m_batchIndex > 0)
    {
        // inference free, but still some task queued, so we need the input
        return -2;
    }
    uint32_t *channelIds = new uint32_t[m_batchNum];
    uint32_t *frameIds = new uint32_t[m_batchNum];
    uint32_t *roiIds = new uint32_t[m_batchNum];
    TRACE("m_busyRequest.size()  %d \n", m_busyRequest.size());
    while (m_busyRequest.size() > 0)
    {
        InferRequest::Ptr curRequest = m_busyRequest.front();
        InferenceEngine::StatusCode status;

        status = curRequest->Wait(IInferRequest::WaitMode::STATUS_ONLY);

        if (status != InferenceEngine::OK)
        {
            break;
        }
        status = curRequest->Wait(IInferRequest::WaitMode::RESULT_READY);

        if (status != InferenceEngine::OK)
        {
            ERRLOG("status %Xx \n", (int)status);
            break;
        }
        for (int i = 0; i < m_batchNum; i ++)
        {
            channelIds[i] = m_channels.front();
            frameIds[i] = m_frames.front();
            roiIds[i] = m_rois.front();
            m_channels.pop();
            m_frames.pop();
            m_rois.pop();
        }
        // curRequest finished

        std::map<std::string, const float*> results;
        for (const auto& name : m_outputsNames)
        {
            const float* result = curRequest->GetBlob(name)->buffer().as<PrecisionTrait<Precision::FP32>::value_type*>();
            results.insert(std::pair<std::string, const float*>(name.c_str(), result));
        }
        int ret = Translate(datas, m_batchNum, (void*)&results, channelIds, frameIds, roiIds);

        for (int i = 0; i < m_batchNum; i ++)
        {
            channels.push_back(channelIds[i]);
            frames.push_back(frameIds[i]);
        }

        m_freeRequest.push(curRequest);
        m_busyRequest.pop();
    }

    delete[] channelIds;
    delete[] frameIds;
    delete[] roiIds;

    if (m_freeRequest.size() == 0)
    {
        // there is no freeRequest, we don't need input
        return 2;
    }
    return 0;
}

