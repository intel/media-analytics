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

#ifndef _CROP_THREAD_BLOCK_H_
#define _CROP_THREAD_BLOCK_H_

#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <stdio.h>
#include <unistd.h>
#include <map>
#include "va/va.h"

class CropThreadBlock : public VAThreadBlock
{
public:
    CropThreadBlock(uint32_t index);
    ~CropThreadBlock();

    CropThreadBlock(const CropThreadBlock&) = delete;
    CropThreadBlock& operator=(const CropThreadBlock&) = delete;

    int Loop();

    inline void SetOutDump(bool flag = true) {m_dumpFlag = flag; }
    inline void SetVASync(bool flag = true) {m_vaSyncFlag = flag; }
    inline void SetOutResolution(uint32_t w, uint32_t h) {m_vpOutWidth = w; m_vpOutHeight = h; }
    inline void SetInputResolution(uint32_t w, uint32_t h) {m_inputWidth = w; m_inputHeight = h; }
    inline void SetOutFormat(uint32_t format) {m_vpOutFormat = format; }
    inline void SetVPMemOutTypeVideo(bool flag = false) {m_vpMemOutTypeVideo = flag; }
    inline void SetKeepAspectRatioFlag(bool flag){m_keepAspectRatio = flag; }
    inline void SetBatchSize(int batchSize) {m_batchSize = batchSize; }
protected:
    int PrepareInternal() override;

    int Crop(VASurfaceID inSurf,
             VASurfaceID outSurf,
             uint32_t srcx,
             uint32_t srcy,
             uint32_t srcw,
             uint32_t srch,
             bool keepRatio);

    bool IsDecodeOutput(VAData *data);
    bool IsRoiRegion(VAData *data);

    int FindFreeOutput();

    FILE *GetDumpFile(uint32_t channel);

    uint32_t m_index;

    uint32_t m_vpOutFormat;
    uint32_t m_vpOutWidth;
    uint32_t m_vpOutHeight;
    uint32_t m_inputWidth;
    uint32_t m_inputHeight;

    uint32_t m_bufferNum;

    uint32_t m_batchSize;

    VAContextID m_contextID;

    VASurfaceID *m_outputVASurfs;
    uint8_t **m_outBuffers;
    int *m_outRefs;

    bool m_dumpFlag;
    bool m_vpMemOutTypeVideo;
    bool m_keepAspectRatio;
    bool m_vaSyncFlag;
    std::map<uint32_t, FILE *> m_dumpFps;
};

#endif
