/*
* Copyright (c) 2021, Intel Corporation
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

#include "InferenceSISR.h"
#include <ie_plugin_config.hpp>

#include "DataPacket.h"

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

inline uint8_t clip(float val)
{
    return (val < 0) ? 0 : ((val>255)? 255: (uint8_t)val);
}

template <typename T>
void matU8ToBlob(const cv::Mat& orig_image, InferenceEngine::Blob::Ptr& blob, int batchIndex = 0) 
{
    InferenceEngine::SizeVector blobSize = blob->getTensorDesc().getDims();
    const size_t width = blobSize[3];
    const size_t height = blobSize[2];
    const size_t channels = blobSize[1];

    InferenceEngine::MemoryBlob::Ptr mblob = InferenceEngine::as<InferenceEngine::MemoryBlob>(blob);
    if (!mblob) 
    {
        printf("Cannot cast inputBlob to MemoryBlob\n");
        return;
    }

    auto mblobHolder = mblob->wmap();
    T *blob_data = mblobHolder.as<T *>();

    cv::Mat resized_image(orig_image);
    if (static_cast<int>(width) != orig_image.size().width ||
            static_cast<int>(height) != orig_image.size().height) 
    {
        cv::resize(orig_image, resized_image, cv::Size(width, height));
    }

    int batchOffset = batchIndex * width * height * channels;

    for (size_t c = 0; c < channels; c++) 
    {
        for (size_t  h = 0; h < height; h++) 
        {
            for (size_t w = 0; w < width; w++) 
            {
                blob_data[batchOffset + c * width * height + h * width + w] =
                        orig_image.at<cv::Vec3b>(h, w)[c];
            }
        }
    }
}

InferenceSISR::InferenceSISR():
    m_inputWidth(0),
    m_inputHeight(0),
    m_channelNum(1),
    m_inputWidth2(0),
    m_inputHeight2(0),
    m_channelNum2(0),
    m_outputWidth(0),
    m_outputHeight(0),
    m_outputChannelNum(0),
    m_resultSize(0)
{
}

InferenceSISR::~InferenceSISR()
{
}

void InferenceSISR::SetDataPorts()
{
	InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
	auto& inputInfoFirst = inputInfo.begin()->second;
	//inputInfoFirst->setPrecision(Precision::U8);
	//inputInfoFirst->getInputData()->setLayout(Layout::NCHW);
    if (m_shareSurfaceWithVA)
    {
        inputInfoFirst->getPreProcess().setColorFormat(ColorFormat::NV12);
    }
	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	//_output->setPrecision(Precision::FP32);
}

int InferenceSISR::Load(const char *device, const char *model, const char *weights)
{
    int ret = InferenceOV::Load(device, model, weights);
    if (ret)
    {
        return ret;
    }

    // Set input parameters
    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    if(inputInfo.size() != 2) {
        printf("ERROR: Input surface number doesn't match the requirement of SISR model");
        return -1;
    }
    auto& inputInfo1 = inputInfo["0"];
    const InferenceEngine::SizeVector inputDims1 = inputInfo1->getTensorDesc().getDims();
    m_channelNum = inputDims1[1];
    m_inputWidth = inputDims1[3];
    m_inputHeight = inputDims1[2];
    auto& inputInfo2 = inputInfo["1"];
    const InferenceEngine::SizeVector inputDims2 = inputInfo2->getTensorDesc().getDims();
    m_channelNum2 = inputDims2[1];
    m_inputWidth2 = inputDims2[3];
    m_inputHeight2 = inputDims2[2];

    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    auto& _output = outputInfo.begin()->second;
    const InferenceEngine::SizeVector outputDims = _output->getTensorDesc().getDims();
    m_outputWidth = outputDims[3];
    m_outputHeight = outputDims[2];
    m_outputChannelNum = outputDims[1];

    return 0;
}

void InferenceSISR::GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc)
{
    *width = m_inputWidth;
    *height = m_inputHeight;
    *fourcc = m_shareSurfaceWithVA ? 0x3231564e : 0x50424752; //MFX_FOURCC_RGBP
}

int InferenceSISR::InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId, uint32_t roiId)
{
    std::vector<cv::Mat> imgPlanes;
    cv::Mat R(m_inputHeight, m_inputWidth, CV_8UC1, (uint8_t*)img);
    cv::Mat G(m_inputHeight, m_inputWidth, CV_8UC1, (uint8_t*)(img + m_inputHeight*m_inputWidth*1));
    cv::Mat B(m_inputHeight, m_inputWidth, CV_8UC1, (uint8_t*)(img + m_inputHeight*m_inputWidth*2));
    imgPlanes.push_back(R);
    imgPlanes.push_back(G);
    imgPlanes.push_back(B);

    cv::Mat resultImg;
    cv::merge(imgPlanes, resultImg);

    InsertImage(resultImg, channelId, frameId, roiId);

    return 0;
}

int InferenceSISR::InsertImage(const cv::Mat &image, uint32_t channelId, uint32_t frameId, uint32_t roiId)
{
    if (m_freeRequest.size() == 0)
    {
        Wait();
        GetOutputInternal(m_internalDatas, m_internalChannels, m_internalFrames);
    }
    InferRequest::Ptr curRequest = m_freeRequest.front();

    int channelNum = 3;
    void *dst = nullptr;

    Blob::Ptr lrInputBlob = curRequest->GetBlob("0");
    matU8ToBlob<float_t>(image, lrInputBlob, 0);

    cv::Mat resizedImg;
    cv::resize(image, resizedImg, image.size()*4, 0, 0, cv::INTER_CUBIC);
    Blob::Ptr cubInputBlob = curRequest->GetBlob("1");
    matU8ToBlob<float_t>(resizedImg, cubInputBlob, 0);

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

void InferenceSISR::CopyImage(const uint8_t *img, void *dst, uint32_t w, uint32_t h, uint32_t c, uint32_t batchIndex)
{
    float *data = (float *)dst;
    for (int z=0; z<c; z++)
        for (int y=0; y<h; y++)
            for (int x=0; x<w; x++)
                data[z*w*h + y*w + x] = img[z*w*h + y*w + x];
}

int InferenceSISR::Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds, uint32_t *roiIds)
{
    std::map<std::string, const float*>* curResults = (std::map<std::string, const float*>*) result;
    float* curResult = (float*)curResults->find(m_outputsNames[0])->second;
    bool outputNV12 = true;
    uint32_t alignedHeight = 0;
    uint32_t planeSize = m_outputHeight * m_outputWidth;;
    uint32_t imgSize = 0;
    uint32_t fourcc;

    for (int i = 0; i < count; i ++) 
    {
        if (outputNV12) 
        { 
            fourcc = 0x3231564e; // MFX_FOURCC_NV12
            alignedHeight = ((m_outputHeight + 15) / 16)*16;
            uint32_t planeSizeNV12 = alignedHeight * m_outputWidth;
            imgSize = 3 * planeSizeNV12 / 2;
            if (m_outImg == nullptr) 
            {
                m_outImg = new uint8_t[imgSize];
            }
            memset(m_outImg, 0, imgSize);
            for (size_t h = 0; h < m_outputHeight; h++) 
            {
                for (size_t w = 0; w < m_outputWidth; w++) 
                {
                    float r = clip(curResult[0 * planeSize + h * m_outputWidth + w] * 255);
                    float g = clip(curResult[1 * planeSize + h * m_outputWidth + w] * 255);
                    float b = clip(curResult[2 * planeSize + h * m_outputWidth + w] * 255);
                    uint8_t y = clip( 0.257 * r + 0.504 * g + 0.098 * b +  16);
                    m_outImg[h * m_outputWidth + w] = y;
                    if (h%2 == 0 && w%2 == 0) 
                    {
                        uint8_t u = clip(-0.148 * r - 0.291 * g + 0.439 * b + 128);
                        uint8_t v = clip( 0.439 * r - 0.368 * g - 0.071 * b + 128);
                        m_outImg[planeSizeNV12 + (h/2) * m_outputWidth + w + 0] = u;
                        m_outImg[planeSizeNV12 + (h/2) * m_outputWidth + w + 1] = v;
                    }
                }
            }
            // debug dump
            if (0) 
            {
                FILE* fp = fopen("sr_4x_.nv12", "wb+");
                fwrite(m_outImg, 1, imgSize, fp);
                fclose(fp);
            }
            
        } 
        else 
        {
            fourcc = 0x50424752; // MFX_FOURCC_RGBP
            alignedHeight = m_outputHeight;
            imgSize = m_outputChannelNum * planeSize;
            m_outImg = new uint8_t[imgSize];
            for (size_t c = 0; c < m_outputChannelNum; c++) 
            {
                for (size_t h = 0; h < m_outputHeight; h++) 
                {
                    for (size_t w = 0; w < m_outputWidth; w++) 
                    {
                        float val = curResult[c * planeSize + h * m_outputWidth + w] * 255;
                        m_outImg[c * planeSize + h * m_outputWidth + w] = (val < 0) ? 0 : ((val>255)? 255: (uint8_t)val);
                    }
                }
            }
            // debug dump
            if (0) 
            {
                std::vector<cv::Mat> imgPlanes = {
                    cv::Mat(m_outputHeight, m_outputWidth, CV_8UC1, &(m_outImg[planeSize * 0])),
                    cv::Mat(m_outputHeight, m_outputWidth, CV_8UC1, &(m_outImg[planeSize * 1])),
                    cv::Mat(m_outputHeight, m_outputWidth, CV_8UC1, &(m_outImg[planeSize * 2]))
                };
                cv::Mat resultImg;
                cv::merge(imgPlanes, resultImg);
                std::string outImgName = std::string("sr_4x_" + std::to_string(i + 1) + ".png");
                cv::imwrite(outImgName, resultImg);
            }
        }

        VAData *data = VAData::Create(m_outImg, m_outputWidth, alignedHeight, m_outputChannelNum, fourcc);
        data->SetID(channelIds[i], frameIds[i]);
        // one roi creates one output, just copy the roiIds
        data->SetRoiIndex(roiIds[i]);
        datas.push_back(data);
        curResult += imgSize;
    }

    return 0;
}