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

#ifndef __INFERRENCE_OPENVINO_H__
#define __INFERRENCE_OPENVINO_H__

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <gpu/gpu_context_api_va.hpp>

#include <vector>
#include <queue>
#include <ie_plugin_config.hpp>
#include <inference_engine.hpp>

#include <ie_compound_blob.h>

#include "Inference.h"

class InferenceOV : public InferenceBlock
{
public:
    InferenceOV();
    virtual ~InferenceOV();

    int Initialize(uint32_t batch_num = 1, uint32_t async_depth = 1, uint32_t stream_num = 0, float confidence_threshold = 0.8, 
                uint32_t model_input_reshape_height = 0, uint32_t model_input_reshape_width = 0);

    // derived classes need to get input dimension and output dimension besides the base Load operation
    virtual int Load(const char *device, const char *model, const char *weights);

    int InsertImage(const cv::Mat &image, uint32_t channelId, uint32_t frameId, uint32_t roiId) { return 0; }

    // the img should already be in format that the model requests, otherwise, do the conversion outside
    int InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId, uint32_t roiId);

    // the img should already be in format that the model requests, otherwise, do the conversion outside
    int InsertImage(const int surfID, uint32_t channelId, uint32_t frameId, uint32_t roiId);

    int Wait();

    int GetOutput(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames);

    void JoinVAContext(void *va_dpy)
    {
        m_vaDisplay = va_dpy;
        m_shareSurfaceWithVA = true;
    }

protected:
    // derived classes need to fill the dst with the img, based on their own different input dimension
    virtual void CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex) = 0;

    // derived classes need to fill VAData by the result, based on their own different output demension
    virtual int Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds, uint32_t *roiIds) = 0;

    // derived classes need to set the input and output info
    virtual void SetDataPorts() = 0;

    virtual uint32_t GetInputWidth() = 0;
    virtual uint32_t GetInputHeight() = 0;

    int GetOutputInternal(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames);
    void IECoreInfo(const char* device);

    std::queue<InferenceEngine::InferRequest::Ptr> m_busyRequest;
    std::queue<InferenceEngine::InferRequest::Ptr> m_freeRequest;

    std::queue<uint64_t> m_ids; // higher 32-bit: channel id, lower 32-bit: frame index
    std::queue<uint32_t> m_channels;
    std::queue<uint32_t> m_frames;
    std::queue<uint32_t> m_rois;

    std::vector<InferenceEngine::Blob::Ptr> m_batchedBlobs;

    uint32_t m_asyncDepth;
    uint32_t m_nStreams;
    uint32_t m_batchNum;
    uint32_t m_modelInputReshapeWidth;
    uint32_t m_modelInputReshapeHeight;
    float m_confidenceThreshold;

    uint32_t m_batchIndex;

    // model related
    std::string m_inputName;
    std::vector<std::string> m_outputsNames;

    // openvino related
    InferenceEngine::Core m_ie;
    InferenceEngine::CNNNetwork m_network;
    InferenceEngine::ExecutableNetwork m_execNetwork;

    // internal buffer to guarantee InsertImage can always find free worker
    std::vector<VAData *> m_internalDatas;
    std::vector<uint32_t> m_internalChannels;
    std::vector<uint32_t> m_internalFrames;

    // VA Display for context joining with VA
    void *m_vaDisplay;
    InferenceEngine::RemoteContext::Ptr m_sharedContext;
    bool m_shareSurfaceWithVA;
};

#endif //__INFERRENCE_OPENVINO_H__
