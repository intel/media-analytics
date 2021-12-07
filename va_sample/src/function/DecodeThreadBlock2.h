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

#ifndef _DECODE_THREAD_2_BLOCK_H_
#define _DECODE_THREAD_2_BLOCK_H_

#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <stdio.h>
#include <unistd.h>
#include <string>

class DecodeThreadBlock : public VAThreadBlock
{
public:
    DecodeThreadBlock(uint32_t channel);
    DecodeThreadBlock(uint32_t channel, MFXVideoSession *ExtMfxSession, mfxFrameAllocator *mfxAllocator);

    ~DecodeThreadBlock();

    void SetDecodeOutputRef(int ref) {m_decodeRefNum = ref; }
    void SetDecPostProc(bool flag)   {m_bEnableDecPostProc = flag; }

    void SetVPOutputRef(int ref) {m_vpRefNum = ref;}

    void SetVPRatio(uint32_t ratio) {m_vpRatio = ratio; }

    void SetVPOutFormat(uint32_t fourcc) {m_vpOutFormat = fourcc; }

    void SetVPOutResolution(uint32_t width, uint32_t height) {m_vpOutWidth = width; m_vpOutHeight = height;}

    void SetCodecType(mfxU32 codec_type){m_CodecId = codec_type;}

    void SetFilterFlag(mfxU32 filter_flag){m_filterflag = (filter_flag == 0 ? MFX_SCALING_MODE_LOWPOWER : MFX_SCALING_MODE_QUALITY);}

    int Loop();

    inline void SetVPOutDump(bool flag = true) {m_vpOutDump = flag; }
    inline void SetVPDumpAllframe(int flag = true){m_vpDumpAllFrame = flag; }
    inline void SetDumpFileName(std::string filename){m_usersetdumpname = filename;}
    inline void SetVPMemOutTypeVideo(bool flag = false) {m_vpMemOutTypeVideo = flag; }
    inline void SetDecodeOutputWithVP(bool flag = true) {m_decodeOutputWithVP = flag; }

    inline void GetDecodeResolution(uint32_t *width, uint32_t *height)
    {
        *width = m_mfxVideoParam.mfx.FrameInfo.CropW;
        *height = m_mfxVideoParam.mfx.FrameInfo.CropH;
    }

    inline void SetFrameNumber(uint32_t num) { m_frameNumber = num; }

protected:
    int PrepareInternal() override;

    int ReadBitStreamData(); // fill the buffer in m_mfxBS, and store the remaining in m_buffer

    int GetFreeVPOutBuffer();
    
    uint32_t m_decodeRefNum;
    uint32_t m_vpRefNum;
    uint32_t m_channel;
    uint32_t m_vpRatio;
    bool     m_bEnableDecPostProc;

    MFXVideoSession *m_mfxSession;
    mfxBitstream m_mfxBS;
    MFXVideoDECODE_VPP *m_mfxDecodeVpp;

    // only used when vp output is in cpu memory
    uint8_t **m_vpOutBuffers;
    uint32_t m_vpOutBufNum;
    int *m_vpOutRefs;

    mfxVideoParam m_mfxVideoParam;
    mfxU32  m_CodecId;

    mfxVideoChannelParam m_mfxVideoChannelParam;
    mfxExtVPPScaling m_scalingConfig;
    mfxExtBuffer *m_vpExtBuf;
    uint32_t  m_filterflag;

    uint8_t *m_buffer;
    uint32_t m_bufferOffset;
    uint32_t m_bufferLength;

    uint32_t m_vpOutFormat;
    uint32_t m_vpOutWidth;
    uint32_t m_vpOutHeight;

    bool m_vpOutDump;
    int  m_vpDumpAllFrame;
    bool m_vpMemOutTypeVideo;

    bool m_decodeOutputWithVP;

    bool m_rgbpWA; // on some platforms RGBP is not supported

    std::string m_usersetdumpname;

    uint32_t m_frameNumber;
};

#endif
