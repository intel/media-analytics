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

#ifndef __INFERRENCE_SISR_H__
#define __INFERRENCE_SISR_H__

#include "InferenceOV.h"

class InferenceSISR : public InferenceOV
{
public:
    InferenceSISR();
    virtual ~InferenceSISR();

    virtual int Load(const char *device, const char *model, const char *weights);

    virtual void GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc);
protected:
    int InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId, uint32_t roiId);
    int InsertImage(const cv::Mat &image, uint32_t channelId, uint32_t frameId, uint32_t roiId);
    void CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex) { return; }
    void CopyImage(const uint8_t *img, void *dst, uint32_t w, uint32_t h, uint32_t c, uint32_t batchIndex);
    int Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channels, uint32_t *frames, uint32_t *roiIds);
    void SetDataPorts();

    uint32_t GetInputWidth() {return m_inputWidth; }
    uint32_t GetInputHeight() {return m_inputHeight; }

    // model related
    uint32_t m_inputWidth;
    uint32_t m_inputHeight;
    uint32_t m_channelNum;

    uint32_t m_inputWidth2;
    uint32_t m_inputHeight2;
    uint32_t m_channelNum2;

    uint32_t m_outputWidth;
    uint32_t m_outputHeight;
    uint32_t m_outputChannelNum;

    uint8_t* m_outImg = nullptr;

    uint32_t m_resultSize; // size per one result
};

#endif //__INFERRENCE_SISR_H__