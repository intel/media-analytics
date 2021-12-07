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

#include "DecodeThreadBlock2.h"
#include "common.h"
#include "logs.h"
#include "Statistics.h"
#include "MfxSessionMgr.h"
#include <iostream>

DecodeThreadBlock::DecodeThreadBlock(uint32_t channel):
    m_decodeRefNum(1),
    m_vpRefNum(1),
    m_channel(channel),
    m_vpRatio(1),
    m_bEnableDecPostProc(false),
    m_mfxSession(nullptr),
    m_mfxDecodeVpp(nullptr),
    m_vpOutBuffers(nullptr),
    m_vpOutBufNum(5),
    m_vpOutRefs(nullptr),
    m_CodecId(MFX_CODEC_AVC),
    m_vpExtBuf(nullptr),
    m_filterflag(MFX_SCALING_MODE_LOWPOWER),
    m_buffer(nullptr),
    m_bufferOffset(0),
    m_bufferLength(0),
    m_vpOutFormat(MFX_FOURCC_RGBP),
    m_vpOutWidth(0),
    m_vpOutHeight(0),
    m_vpOutDump(false),
    m_vpDumpAllFrame(0),
    m_vpMemOutTypeVideo(false),
    m_decodeOutputWithVP(false),
    m_rgbpWA(false),
    m_frameNumber(0)
{
    TRACE("");
    memset(&m_mfxBS, 0, sizeof(m_mfxBS));
    memset(&m_mfxVideoParam, 0, sizeof(m_mfxVideoParam));
    memset(&m_mfxVideoChannelParam, 0, sizeof(m_mfxVideoChannelParam));
    // allocate the buffer
    m_buffer = new uint8_t[1024 * 1024];
}

DecodeThreadBlock::DecodeThreadBlock(uint32_t channel, MFXVideoSession *externalMfxSession, mfxFrameAllocator *mfxAllocator):
    DecodeThreadBlock(channel)
{
    TRACE("");
    m_mfxSession = externalMfxSession;
}

DecodeThreadBlock::~DecodeThreadBlock()
{
    TRACE("");
    MfxSessionMgr::getInstance().Clear(m_channel);
    delete[] m_buffer;

    if (m_vpOutBuffers)
    {
        for (int i = 0; i < m_vpOutBufNum; i++)
        {
            delete[] m_vpOutBuffers[i];
        }
        delete[] m_vpOutBuffers;
    }

    if (m_vpOutRefs)
    {
        delete[] m_vpOutRefs;
    }
}

int DecodeThreadBlock::PrepareInternal()
{
    INFO("");
    mfxStatus sts;
    if (m_mfxSession == nullptr)
    {
        m_mfxSession = MfxSessionMgr::getInstance().GetSession(m_channel);
    }

    m_mfxDecodeVpp = new MFXVideoDECODE_VPP(*m_mfxSession);

    m_mfxVideoParam.mfx.CodecId = m_CodecId;
    m_mfxVideoParam.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    m_mfxVideoParam.mfx.FrameInfo.ChannelID = 0;

    // Prepare media sdk bit stream buffer
    // - Arbitrary buffer size for this example        
    memset(&m_mfxBS, 0, sizeof(m_mfxBS));
    m_mfxBS.MaxLength = 1024 * 1024;
    m_mfxBS.Data = new mfxU8[m_mfxBS.MaxLength];
    MSDK_CHECK_POINTER(m_mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    // Prepare media sdk decoder parameters
    ReadBitStreamData();

    sts = m_mfxDecodeVpp->DecodeHeader(&m_mfxBS, &m_mfxVideoParam);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    INFO("Done preparing video decoder parameters.\n");

    mfxVideoChannelParam *channelParams = nullptr;
    uint32_t numChannel = 0;

    uint32_t scalingMode = MFX_SCALING_MODE_DEFAULT;
    if (m_bEnableDecPostProc)
    {
        scalingMode = MFX_SCALING_MODE_INTEL_GEN_VDBOX;
    }
    else if (m_filterflag == MFX_SCALING_MODE_QUALITY)
    {
        scalingMode = MFX_SCALING_MODE_INTEL_GEN_COMPUTE;
    }
    else
    {
        scalingMode = MFX_SCALING_MODE_INTEL_GEN_VEBOX;
    }

    // Prepare media sdk vp parameters
    if (m_vpRatio > 0)
    {
        m_mfxVideoChannelParam.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        m_mfxVideoChannelParam.vpp = m_mfxVideoParam.mfx.FrameInfo;
        m_mfxVideoChannelParam.vpp.ChannelID = 1;
        m_mfxVideoChannelParam.vpp.CropW = m_vpOutWidth;
        m_mfxVideoChannelParam.vpp.CropH = m_vpOutHeight;
        m_mfxVideoChannelParam.vpp.Width = MSDK_ALIGN16(m_mfxVideoChannelParam.vpp.CropW);
        m_mfxVideoChannelParam.vpp.Height = MSDK_ALIGN16(m_mfxVideoChannelParam.vpp.CropH);
        m_mfxVideoChannelParam.vpp.FourCC = m_vpOutFormat;
        if (m_vpOutFormat == MFX_FOURCC_RGBP && m_rgbpWA)
        {
            m_mfxVideoChannelParam.vpp.FourCC = MFX_FOURCC_RGB4;
        }
        m_mfxVideoChannelParam.vpp.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        if (m_vpOutFormat == MFX_FOURCC_NV12)
        {
            m_mfxVideoChannelParam.vpp.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }

        m_scalingConfig.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
        m_scalingConfig.Header.BufferSz = sizeof(mfxExtVPPScaling);
        m_scalingConfig.ScalingMode = scalingMode;
        INFO("scaling mode is %d\n", scalingMode);
        m_vpExtBuf = &(m_scalingConfig.Header);
        m_mfxVideoChannelParam.ExtParam = (mfxExtBuffer **)&m_vpExtBuf;
        m_mfxVideoChannelParam.NumExtParam = 1;

        channelParams = &m_mfxVideoChannelParam;
        numChannel = 1;
    }

    sts = m_mfxDecodeVpp->Init(&m_mfxVideoParam, &channelParams, numChannel);

    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_vpOutRefs = new int[m_vpOutBufNum];
    memset(m_vpOutRefs, 0, sizeof(int)*m_vpOutBufNum);
    m_vpOutBuffers = new uint8_t *[m_vpOutBufNum];
    memset(m_vpOutBuffers, 0, sizeof(uint8_t *)*m_vpOutBufNum);

    for (int i = 0; i < m_vpOutBufNum; i++)
    {
        // allocate system buffer to store VP output
        m_vpOutBuffers[i] = new uint8_t[3*m_vpOutWidth*m_vpOutHeight]; // RGBP
        memset(m_vpOutBuffers[i], 0, 3*m_vpOutWidth*m_vpOutHeight);
    }

    return 0;
}

int DecodeThreadBlock::ReadBitStreamData()
{
    TRACE("");
    memmove(m_mfxBS.Data, m_mfxBS.Data + m_mfxBS.DataOffset, m_mfxBS.DataLength);
    m_mfxBS.DataOffset = 0;
    uint32_t targetLen = m_mfxBS.MaxLength - m_mfxBS.DataLength;

    if (m_bufferLength > targetLen)
    {
        memcpy(m_mfxBS.Data + m_mfxBS.DataLength, m_buffer + m_bufferOffset, targetLen);
        uint32_t remainingSize = m_bufferLength - targetLen;
        m_bufferOffset += targetLen;
        m_bufferLength = remainingSize;
        m_mfxBS.DataLength += targetLen;
        return 0;
    }

    uint32_t copiedLen = 0;
    if (m_bufferLength > 0)
    {
        memcpy(m_mfxBS.Data + m_mfxBS.DataLength, m_buffer + m_bufferOffset, m_bufferLength);
        m_bufferLength = 0;
        m_bufferOffset = 0;
        m_mfxBS.DataLength += m_bufferLength;
        copiedLen += m_bufferLength;
    }

    while (m_mfxBS.DataLength < m_mfxBS.MaxLength)
    {
        VADataPacket *packet = AcquireInput();
        VAData *data = packet->front();
        uint8_t *src = data->GetSurfacePointer();
        uint32_t len, offset;
        data->GetBufferInfo(&offset, &len);
        ReleaseInput(packet);
        if (len == 0)
        {
            data->DeRef();
	    m_mfxBS.DataFlag |= MFX_BITSTREAM_EOS;
            return (copiedLen == 0)?MFX_ERR_MORE_DATA:0;
        }
        if (len > (m_mfxBS.MaxLength - m_mfxBS.DataLength))
        {
            uint32_t copyLen = m_mfxBS.MaxLength - m_mfxBS.DataLength;
            uint32_t remainingLen = len - copyLen;
            memcpy(m_mfxBS.Data + m_mfxBS.DataLength, src + offset, copyLen);
            m_mfxBS.DataLength += copyLen;
            copiedLen += copyLen;
            memcpy(m_buffer, src + offset + copyLen, remainingLen);
            m_bufferLength = remainingLen;
        }
        else
        {
            memcpy(m_mfxBS.Data + m_mfxBS.DataLength, src + offset, len);
            m_mfxBS.DataLength += len;
            copiedLen += len;
        }
        data->DeRef();
    }

    return 0;
}

int DecodeThreadBlock::Loop()
{
    TRACE("");
    mfxStatus sts = MFX_ERR_NONE;
    uint32_t nDecoded = 0;
    FILE* fp_dumpall;
    if(m_vpDumpAllFrame){
        if(!m_usersetdumpname.empty())
        {
            fp_dumpall = fopen(m_usersetdumpname.c_str(), "wb");
        }
        else
        {
            char filename[256];
            if (m_vpOutFormat == MFX_FOURCC_NV12)
                sprintf(filename, "VPOut_%d.%dx%d.nv12", m_channel, m_vpOutWidth, m_vpOutHeight);
            else
                sprintf(filename, "VPOut_%d.%dx%d.rgbp", m_channel, m_vpOutWidth, m_vpOutHeight);
            fp_dumpall = fopen(filename, "wb");
        }
        if(fp_dumpall == NULL)
            ERRLOG("Create dump.yuv failed\n");
    }

    mfxSurfaceArray* mfxOutSurfaceArr = NULL;
    while ((MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts))
    {
        bool isVpNeeded = (m_vpRatio > 0) && ((nDecoded %m_vpRatio) == 0);
        bool isVpSkipped = (m_vpRatio > 0) && ((nDecoded %m_vpRatio) != 0);
        if (m_stop)
        {
            break;
        }
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            usleep(1000); // Wait if device is busy, then repeat the same call to DecodeFrameAsync
        }
        if (MFX_ERR_MORE_DATA == sts)
        {
            sts = (mfxStatus)ReadBitStreamData(); // doesn't return if meets the end of stream, try again
            if (sts != 0)
            {
                sts = (mfxStatus)ReadBitStreamData();
            }
            MSDK_BREAK_ON_ERROR(sts);
        }

        if (isVpSkipped)
        {
            // need skip channel to skip VP here
            uint32_t skipChnlID = 1;
            sts = m_mfxDecodeVpp->DecodeFrameAsync(&m_mfxBS, &skipChnlID, 1, &mfxOutSurfaceArr);
        }
        else
        {
            sts = m_mfxDecodeVpp->DecodeFrameAsync(&m_mfxBS, nullptr, 0, &mfxOutSurfaceArr);
        }

        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            continue;
        }

        if (sts < MFX_ERR_NONE && sts != MFX_ERR_NONE_PARTIAL_OUTPUT)
        {
            continue;
        }

        ++ nDecoded;
        Statistics::getInstance().Step(DECODED_FRAMES);

        VADataPacket *outputPacket = DequeueOutput();
        // decoder output surface
        if (mfxOutSurfaceArr->NumSurfaces > 0)
        {
            int curDecRef = m_decodeRefNum;
            if (m_decodeOutputWithVP && isVpNeeded)
            {
                ++ curDecRef;
            }

            mfxFrameSurface1 *pSurface = mfxOutSurfaceArr->Surfaces[0];
            
            mfxHDL handle;
            mfxResourceType type;
            pSurface->FrameInterface->GetNativeHandle(pSurface, &handle, &type);
            
            // output
            if (curDecRef > 0) // decode output is needed
            {
                VAData *vaData = VAData::Create(pSurface);
                vaData->SetID(m_channel, nDecoded);
                vaData->SetRef(curDecRef);
                outputPacket->push_back(vaData);
            }

            // dump
            if (m_vpDumpAllFrame == 2 && fp_dumpall)
            {
                pSurface->FrameInterface->Map(pSurface, MFX_MAP_READ);
                mfxFrameInfo *pInfo = &pSurface->Info;
                mfxFrameData *pData = &pSurface->Data;

                uint8_t* ptr;
                uint32_t w, h;
                if (pInfo->CropH > 0 && pInfo->CropW > 0)
                {
                    w = pInfo->CropW;
                    h = pInfo->CropH;
                }
                else
                {
                    w = pInfo->Width;
                    h = pInfo->Height;
                }


                if(pInfo->FourCC == MFX_FOURCC_NV12)
                {
                    ptr	= pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    for(int i = 0; i < h; i++)
                    {
    				    fwrite(ptr + i*pData->Pitch, 1, w, fp_dumpall);
                    }

                    ptr	= pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    for(int i = 0; i < h / 2; i++)
                    {
    				    fwrite(ptr + i*pData->Pitch, 1, w, fp_dumpall);
                    }
                }

                pSurface->FrameInterface->Unmap(pSurface);
            }

            if (curDecRef == 0) // decode output is not needed
            {
                pSurface->FrameInterface->Release(pSurface);
            }
        }

        // vp output surface
        if (mfxOutSurfaceArr->NumSurfaces > 1)
        {
            int index = -1;
            uint32_t w = m_vpOutWidth;
            uint32_t h = m_vpOutHeight;
            if (!isVpNeeded)
            {
                ERRLOG("Error, VP should be involved if output multiple surfaces\n");
            }

            mfxFrameSurface1 *pSurface = mfxOutSurfaceArr->Surfaces[1];

            // lock and dump
            bool isLockNeeded = ((m_vpRefNum > 0) && !m_vpMemOutTypeVideo) 
                                || (m_vpDumpAllFrame == 1 && fp_dumpall)
                                || (m_vpOutDump);

            if (isLockNeeded)
            {
                while(true)
                {
                    index = GetFreeVPOutBuffer();
                    if (index < 0)
                    {
                        usleep(1000);
                    }
                    else
                    {
                        break;
                    }
                }
                
                pSurface->FrameInterface->Map(pSurface, MFX_MAP_READ);
                mfxFrameInfo *pInfo = &pSurface->Info;
                mfxFrameData *pData = &pSurface->Data;
                
                
                uint8_t* ptr;
                if (pInfo->CropH > 0 && pInfo->CropW > 0)
                {
                    w = pInfo->CropW;
                    h = pInfo->CropH;
                }
                else
                {
                    w = pInfo->Width;
                    h = pInfo->Height;
                }
                
                if(m_vpOutFormat == MFX_FOURCC_NV12)
                {
                    ptr = pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    uint8_t *pTemp = m_vpOutBuffers[index];
                
                    for(int i = 0; i < h; i++)
                    {
                        memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
                    }
                
                    ptr = pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    pTemp = m_vpOutBuffers[index] + w*h;
                    for(int i = 0; i < h / 2; i++)
                    {
                       memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
                    }
                }
                
                else if(!m_rgbpWA)
                {
                    uint8_t *pTemp = m_vpOutBuffers[index];
                    ptr   = pData->B + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                
                    for (int i = 0; i < h; i++)
                    {
                       memcpy(pTemp + i*w, ptr + i*pData->Pitch, w);
                    }
                
                    ptr = pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    pTemp = m_vpOutBuffers[index] + w*h;
                    for(int i = 0; i < h; i++)
                    {
                       memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
                    }
                
                    ptr = pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    pTemp = m_vpOutBuffers[index] + 2*w*h;
                    for(int i = 0; i < h; i++)
                    {
                        memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
                    }
                }
                else
                {
                    uint8_t *pbuffer = new uint8_t[pData->Pitch*h];
                    memcpy(pbuffer, pData->B, pData->Pitch * h);
                
                    uint8_t *ptrR = m_vpOutBuffers[index];
                    uint8_t *ptrG = ptrR + w * h;
                    uint8_t *ptrB = ptrG + w * h;
                
                    for (int i = 0; i < h; i ++)
                    {
                        for (int j = 0; j < w; j ++)
                        {
                            ptrB[i*w+j] = pbuffer[i * pData->Pitch + j*4];
                            ptrG[i*w+j] = pbuffer[i * pData->Pitch + j*4 + 1];
                            ptrR[i*w+j] = pbuffer[i * pData->Pitch + j*4 + 2];
                        }
                    }
                    delete[] pbuffer;
                }
                pSurface->FrameInterface->Unmap(pSurface);
            }

            // output
            if (m_vpRefNum)
            {
                if (m_vpMemOutTypeVideo)
                {
                    VAData *vaData = VAData::Create(pSurface);
                    // Increasing Ref counter manually
                    vaData->SetRef(m_vpRefNum);
                    vaData->SetID(m_channel, nDecoded);
                    outputPacket->push_back(vaData);
                }
                else
                {
                    pSurface->FrameInterface->Release(pSurface);
                    VAData *vaData = VAData::Create(m_vpOutBuffers[index], w, h, w, m_vpOutFormat);
                    vaData->SetExternalRef(&m_vpOutRefs[index]); 
                    vaData->SetID(m_channel, nDecoded);
                    vaData->SetRef(m_vpRefNum);
                    outputPacket->push_back(vaData);
                }
            }
            else
            {
                pSurface->FrameInterface->Release(pSurface);
            }
            

            // dump
            if(m_vpDumpAllFrame == 1  && fp_dumpall){
                if(m_vpOutFormat == MFX_FOURCC_NV12)
                    fwrite(m_vpOutBuffers[index], 1, m_vpOutWidth * m_vpOutHeight * 3 /2, fp_dumpall);
                else
                    fwrite(m_vpOutBuffers[index], 1, m_vpOutWidth * m_vpOutHeight * 3, fp_dumpall);
            }
            
            if (m_vpOutDump)
            {
                char filename[256];
                sprintf(filename, "VPOut_%d_%d.%dx%d.rgbp", m_channel, nDecoded, m_vpOutWidth, m_vpOutHeight);
                FILE *fp = fopen(filename, "wb");
                if(m_vpOutFormat == MFX_FOURCC_NV12)
                    fwrite(m_vpOutBuffers[index], 1, m_vpOutWidth * m_vpOutHeight * 3 / 2, fp);
                else
                    fwrite(m_vpOutBuffers[index], 1, m_vpOutWidth * m_vpOutHeight * 3, fp);
                fclose(fp);
            }

            
        }

        
        EnqueueOutput(outputPacket);

        if (m_frameNumber != 0 && nDecoded >= m_frameNumber)
        {
            break;
        }
        
    }

    if(m_vpDumpAllFrame && fp_dumpall)
        fclose(fp_dumpall);

    Statistics::getInstance().CountDown();

    return 0;
}

int DecodeThreadBlock::GetFreeVPOutBuffer()
{
    for (int i = 0; i < m_vpOutBufNum; i++)
    {
        if (m_vpOutRefs[i] == 0)
        {
            return i;
        }
    }
    return -1;
}

