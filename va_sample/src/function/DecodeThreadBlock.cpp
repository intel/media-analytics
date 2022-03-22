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

#include "DecodeThreadBlock.h"
#include "common.h"
#include "logs.h"
#include "Statistics.h"
#include "MfxSessionMgr.h"
#include <iostream>

using namespace std;

DecodeThreadBlock::DecodeThreadBlock(uint32_t channel):
    m_channel(channel),
    m_decodeRefNum(1),
    m_vpRefNum(1),
    m_vpRatio(1),
    m_bEnableDecPostProc(false),
    m_bEnableTwoPassesScaling(false),
    m_mfxSession(nullptr),
    m_mfxAllocator(nullptr),
    m_mfxDecode(nullptr),
    m_mfxVpp(nullptr),
    m_decodeSurfaces(nullptr),
    m_vpInSurface(nullptr),
    m_vpOutSurfaces(nullptr),
    m_vpOutBuffers(nullptr),
    m_decodeSurfNum(0),
    m_vpInSurfNum(0),
    m_vpOutSurfNum(0),
    m_decodeExtBuf(nullptr),
    m_vpExtBuf(nullptr),
    m_buffer(nullptr),
    m_bufferOffset(0),
    m_bufferLength(0),
    m_vpOutFormat(MFX_FOURCC_RGBP),
    m_CodecId(MFX_CODEC_AVC),
    m_filterflag(MFX_SCALING_MODE_LOWPOWER),
    m_vpOutWidth(0),
    m_vpOutHeight(0),
    m_vpOutWidth_1stPass(0),
    m_vpOutHeight_1stPass(0),
    m_decOutRefs(nullptr),
    m_vpOutRefs(nullptr),
    m_vpOutDump(false),
    m_vpDumpAllFrame(0),
    m_vpMemOutTypeVideo(false),
    m_batchSize(1),
    m_decodeOutputWithVP(false),
    m_rgbpWA(false),
    m_frameNumber(0)
{
    memset(&m_decParams, 0, sizeof(m_decParams));
    memset(&m_vppParams, 0, sizeof(m_vppParams));
    memset(&m_scalingConfig, 0, sizeof(m_scalingConfig));
    memset(&m_decVideoProcConfig, 0, sizeof(m_decVideoProcConfig));
    // allocate the buffer
    m_buffer = new uint8_t[1024 * 1024];
}

DecodeThreadBlock::DecodeThreadBlock(uint32_t channel, MFXVideoSession *externalMfxSession, mfxFrameAllocator *mfxAllocator):
    DecodeThreadBlock(channel)
{
    m_mfxSession = externalMfxSession;
    m_mfxAllocator = mfxAllocator;
}

DecodeThreadBlock::~DecodeThreadBlock()
{
    MfxSessionMgr::getInstance().Clear(m_channel);

    delete[] m_buffer;
    delete[] m_mfxBS.Data;
    delete[] m_decOutRefs;
    delete[] m_vpOutRefs;

    for (int i = 0; i < m_decodeSurfNum; i++)
    {
            delete m_decodeSurfaces[i];
    }
    delete[] m_decodeSurfaces;

    for (int i = 0; i < m_vpOutSurfNum; i++)
    {
            delete m_vpOutSurfaces[i];
            delete m_vpOutBuffers[i];
    }
    delete[] m_vpOutSurfaces;
    delete[] m_vpOutBuffers;
}


int DecodeThreadBlock::PrepareInternal()
{
    mfxStatus sts;
    if (m_mfxSession == nullptr)
    {
        m_mfxSession = MfxSessionMgr::getInstance().GetSession(m_channel);
        if (m_mfxSession == nullptr)
        {
            ERRLOG("Fail to get MSDK session!\n");
            return -1;
        }
    }
    if (m_mfxAllocator == nullptr)
    {
        m_mfxAllocator = MfxSessionMgr::getInstance().GetAllocator(m_channel);
        if (m_mfxAllocator == nullptr)
        {
            ERRLOG("Fail to get MSDK allocator!\n");
            return -1;
        }
    }

    m_mfxDecode = new MFXVideoDECODE(*m_mfxSession);
    m_mfxVpp = new MFXVideoVPP(*m_mfxSession);

    m_decParams.mfx.CodecId = m_CodecId;
    m_decParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    INFO("\t\t.. Suppport H.264(AVC) and use video memory.\n");

    // Prepare media sdk bit stream buffer
    // - Arbitrary buffer size for this example        
    memset(&m_mfxBS, 0, sizeof(m_mfxBS));
    m_mfxBS.MaxLength = 1024 * 1024;
    m_mfxBS.Data = new mfxU8[m_mfxBS.MaxLength];
    MSDK_CHECK_POINTER(m_mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    // Prepare media sdk decoder parameters
    ReadBitStreamData();

    sts = m_mfxDecode->DecodeHeader(&m_mfxBS, &m_decParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    INFO("\t. Done preparing video parameters.\n");

    // Prepare media sdk vp parameters
    m_vppParams.ExtParam = (mfxExtBuffer **)&m_vpExtBuf;
    m_scalingConfig.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
    m_scalingConfig.Header.BufferSz = sizeof(mfxExtVPPScaling);
    m_scalingConfig.ScalingMode = m_filterflag;
    m_vppParams.ExtParam[0] = (mfxExtBuffer*)&(m_scalingConfig);
    m_vppParams.NumExtParam = 1;

    // Video processing input data format / decoded frame information
    m_vppParams.vpp.In.FourCC         = MFX_FOURCC_NV12;
    m_vppParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
    m_vppParams.vpp.In.CropX          = 0;
    m_vppParams.vpp.In.CropY          = 0;
    m_vppParams.vpp.In.CropW          = m_decParams.mfx.FrameInfo.CropW;
    m_vppParams.vpp.In.CropH          = m_decParams.mfx.FrameInfo.CropH;
    m_vppParams.vpp.In.PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
    m_vppParams.vpp.In.FrameRateExtN  = 30;
    m_vppParams.vpp.In.FrameRateExtD  = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_vppParams.vpp.In.Width  = MSDK_ALIGN16(m_vppParams.vpp.In.CropW);
    m_vppParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_vppParams.vpp.In.PicStruct)?
                              MSDK_ALIGN16(m_vppParams.vpp.In.CropH) : MSDK_ALIGN32(m_vppParams.vpp.In.CropH);
    
    // Video processing output data format / resized frame information for inference engine
    m_vppParams.vpp.Out.FourCC        = m_vpOutFormat;
    if (m_vppParams.vpp.Out.FourCC == MFX_FOURCC_RGBP && m_rgbpWA)
    {
        m_vppParams.vpp.Out.FourCC = MFX_FOURCC_RGB4;
    }
    m_vppParams.vpp.Out.ChromaFormat  = MFX_CHROMAFORMAT_YUV444;
    /* Extra Check for chroma format */
    if (MFX_FOURCC_NV12 == m_vppParams.vpp.Out.FourCC)
        m_vppParams.vpp.Out.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
    m_vppParams.vpp.Out.CropX         = 0;
    m_vppParams.vpp.Out.CropY         = 0;
    m_vppParams.vpp.Out.CropW         = m_vpOutWidth;
    m_vppParams.vpp.Out.CropH         = m_vpOutHeight;
    m_vppParams.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
    m_vppParams.vpp.Out.FrameRateExtN = 30;
    m_vppParams.vpp.Out.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_vppParams.vpp.Out.Width  = MSDK_ALIGN16(m_vppParams.vpp.Out.CropW);
    m_vppParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_vppParams.vpp.Out.PicStruct)?
                               MSDK_ALIGN16(m_vppParams.vpp.Out.CropH) : MSDK_ALIGN32(m_vppParams.vpp.Out.CropH);
    
    m_vppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    m_bEnableTwoPassesScaling = false;
    if (m_bEnableDecPostProc)
    {
        if ((MFX_PICSTRUCT_PROGRESSIVE == m_decParams.mfx.FrameInfo.PicStruct))  /* ...And only for progressive!*/
        {   /* it is possible to use decoder's post-processing */
        	m_decVideoProcConfig.Header.BufferId    = MFX_EXTBUFF_DEC_VIDEO_PROCESSING;
        	m_decVideoProcConfig.Header.BufferSz    = sizeof(mfxExtDecVideoProcessing);
        	m_decVideoProcConfig.In.CropX = 0;
        	m_decVideoProcConfig.In.CropY = 0;
        	m_decVideoProcConfig.In.CropW = m_decParams.mfx.FrameInfo.CropW;
        	m_decVideoProcConfig.In.CropH = m_decParams.mfx.FrameInfo.CropH;

        	m_decVideoProcConfig.Out.FourCC = m_decParams.mfx.FrameInfo.FourCC;
        	m_decVideoProcConfig.Out.ChromaFormat = m_decParams.mfx.FrameInfo.ChromaFormat;

            //VD-SFC can do max 8x scaling.  If more than 8x, remaining scaling will be done in VPP as 2nd pass.
            bool bExceedVDSFC8xScalingWidth = (m_decParams.mfx.FrameInfo.CropW / 8.0) > m_vppParams.vpp.Out.CropW;
            bool bExceedVDSFC8xScalingHeight = (m_decParams.mfx.FrameInfo.CropH / 8.0) > m_vppParams.vpp.Out.CropH;

            if (bExceedVDSFC8xScalingWidth || bExceedVDSFC8xScalingHeight)
            {
                m_bEnableTwoPassesScaling = true;
                m_vpOutWidth_1stPass = MSDK_ALIGN16(m_vppParams.vpp.Out.Width);
                m_vpOutHeight_1stPass = (MFX_PICSTRUCT_PROGRESSIVE == m_vppParams.vpp.Out.PicStruct) ?
                    MSDK_ALIGN16(m_vppParams.vpp.Out.Height):
                    MSDK_ALIGN32(m_vppParams.vpp.Out.Height);
                if (bExceedVDSFC8xScalingWidth)
                {
                    m_vpOutWidth_1stPass = MSDK_ALIGN16((m_decParams.mfx.FrameInfo.CropW + 7) / 8);
                }
                if (bExceedVDSFC8xScalingHeight)
                {
                    m_vpOutHeight_1stPass = (MFX_PICSTRUCT_PROGRESSIVE == m_vppParams.vpp.Out.PicStruct) ?
                        MSDK_ALIGN16((m_decParams.mfx.FrameInfo.CropH + 7) / 8) :
                        MSDK_ALIGN32((m_decParams.mfx.FrameInfo.CropH + 7) / 8);
                }
            }

            if (m_bEnableTwoPassesScaling)
            {
                m_decVideoProcConfig.Out.Width = m_vpOutWidth_1stPass;
                m_decVideoProcConfig.Out.Height = m_vpOutHeight_1stPass;
                m_decVideoProcConfig.Out.CropX = 0;
                m_decVideoProcConfig.Out.CropY = 0;
                m_decVideoProcConfig.Out.CropW = m_vpOutWidth_1stPass;
                m_decVideoProcConfig.Out.CropH = m_vpOutHeight_1stPass;
                /* need to correct VPP params: re-size done after decoding
                * So, VPP for CSC NV12->RGBP only */
                m_vppParams.vpp.In.Width = m_vpOutWidth_1stPass;
                m_vppParams.vpp.In.Height = m_vpOutHeight_1stPass;
                m_vppParams.vpp.In.CropW = m_vppParams.vpp.Out.CropW;
                m_vppParams.vpp.In.CropH = m_vppParams.vpp.Out.CropH;
            }
            else
            {
                m_decVideoProcConfig.Out.Width = MSDK_ALIGN16(m_vppParams.vpp.Out.Width);
                m_decVideoProcConfig.Out.Height = MSDK_ALIGN16(m_vppParams.vpp.Out.Height);
                m_decVideoProcConfig.Out.CropX = 0;
                m_decVideoProcConfig.Out.CropY = 0;
                m_decVideoProcConfig.Out.CropW = m_vppParams.vpp.Out.CropW;
                m_decVideoProcConfig.Out.CropH = m_vppParams.vpp.Out.CropH;
                /* need to correct VPP params: re-size done after decoding
                * So, VPP for CSC NV12->RGBP only */
                m_vppParams.vpp.In.Width = m_decVideoProcConfig.Out.Width;
                m_vppParams.vpp.In.Height = m_decVideoProcConfig.Out.Height;
                m_vppParams.vpp.In.CropW = m_vppParams.vpp.Out.CropW;
                m_vppParams.vpp.In.CropH = m_vppParams.vpp.Out.CropH;
                /* scaling is off (it was configured via extended buffer)*/
                m_vppParams.NumExtParam = 0;
                m_vppParams.ExtParam = NULL;
            }

            m_decParams.ExtParam = (mfxExtBuffer **)&m_decodeExtBuf;
            m_decParams.ExtParam[0] = (mfxExtBuffer*)&(m_decVideoProcConfig);
            m_decParams.NumExtParam = 1;
            //std::cout << "\t.Decoder's post-processing is used for resizing. If 2 passes is needed, the 2nd pass is done in VPP\n"<< std::endl;
            INFO("  \t.Decoder's post-processing is used for resizing. If 2 passes is needed, the 2nd pass is done in VPP\n");
        }
    }

    // [decoder]
    // Query number of required surfaces
    mfxFrameAllocRequest DecRequest = { 0 };
    sts = m_mfxDecode->QueryIOSurf(&m_decParams, &DecRequest);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    
    DecRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;
    DecRequest.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
    
    // we should allocate more surfaces for batching
    DecRequest.NumFrameMin += (2 + (m_batchSize > 1) ? m_batchSize * 4: 0);
    DecRequest.NumFrameSuggested += (2 + (m_batchSize > 1) ? m_batchSize * 4: 0);

    // adjust vdsfc output buffer size
    if (m_bEnableDecPostProc)
    {
        DecRequest.Info.Height = m_decVideoProcConfig.Out.Height;
        DecRequest.Info.Width = m_decVideoProcConfig.Out.Width;
        DecRequest.Info.CropW = m_decVideoProcConfig.Out.CropW;
        DecRequest.Info.CropH = m_decVideoProcConfig.Out.CropH;
    }

    // memory allocation
    mfxFrameAllocResponse DecResponse = { 0 };
    sts = m_mfxAllocator->Alloc(m_mfxAllocator->pthis, &DecRequest, &DecResponse);
    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
        return sts;
    }
    m_decodeSurfNum = DecResponse.NumFrameActual;
    // this surface will be used when decodes encoded stream
    m_decodeSurfaces = new mfxFrameSurface1 * [m_decodeSurfNum];
    if(!m_decodeSurfaces)
    {
        MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
        return -1;
    }

    for (int i = 0; i < m_decodeSurfNum; i++)
    {
        m_decodeSurfaces[i] = new mfxFrameSurface1;
        memset(m_decodeSurfaces[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_decodeSurfaces[i]->Info), &(m_decParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bEnableDecPostProc)
        {
            m_decodeSurfaces[i]->Info.Height = m_decVideoProcConfig.Out.Height;
            m_decodeSurfaces[i]->Info.Width  = m_decVideoProcConfig.Out.Width;
            m_decodeSurfaces[i]->Info.CropW  = m_decVideoProcConfig.Out.CropW;
            m_decodeSurfaces[i]->Info.CropH  = m_decVideoProcConfig.Out.CropH;
        }

        // external allocator used - provide just MemIds
        m_decodeSurfaces[i]->Data.MemId = DecResponse.mids[i];
    }

    // [VPP]
    // query input and output surface number
    mfxFrameAllocRequest VPPRequest[2];// [0] - in, [1] - out
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
    sts = m_mfxVpp->QueryIOSurf(&m_vppParams, VPPRequest);

    // [VPP input]
    // use decode output as vpp input
    // each vp input (decode output) has an external reference count
    m_decOutRefs = new int[m_decodeSurfNum];
    memset(m_decOutRefs, 0, sizeof(int)*m_decodeSurfNum);

    // [VPP output]
    mfxFrameAllocResponse VPP_Out_Response = { 0 };
    memcpy(&VPPRequest[1].Info, &(m_vppParams.vpp.Out), sizeof(mfxFrameInfo));    // allocate VPP output frame information

    if (m_batchSize > 1) 
    {
        VPPRequest[1].NumFrameMin = m_batchSize * 4;
        VPPRequest[1].NumFrameSuggested = m_batchSize * 4;
    }

    sts = m_mfxAllocator->Alloc(m_mfxAllocator->pthis, &(VPPRequest[1]), &VPP_Out_Response);

    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
        return 1;
    }
    m_vpOutSurfNum = VPP_Out_Response.NumFrameActual;
    m_vpOutSurfaces = new mfxFrameSurface1 * [m_vpOutSurfNum];
    m_vpOutRefs = new int[m_vpOutSurfNum];
    memset(m_vpOutRefs, 0, sizeof(int)*m_vpOutSurfNum);
    m_vpOutBuffers = new uint8_t *[m_vpOutSurfNum];
    memset(m_vpOutBuffers, 0, sizeof(uint8_t *)*m_vpOutSurfNum);
    
    if(!m_vpOutSurfaces)
    {
        MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
        return 1;
    }
    
    for (int i = 0; i < m_vpOutSurfNum; i++)
    {
        m_vpOutSurfaces[i] = new mfxFrameSurface1;
        memset(m_vpOutSurfaces[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_vpOutSurfaces[i]->Info), &(m_vppParams.vpp.Out), sizeof(mfxFrameInfo));
    
        // external allocator used - provide just MemIds
        m_vpOutSurfaces[i]->Data.MemId = VPP_Out_Response.mids[i];

        // allocate system buffer to store VP output
        m_vpOutBuffers[i] = new uint8_t[3*m_vpOutWidth*m_vpOutHeight]; // RGBP
        memset(m_vpOutBuffers[i], 0, 3*m_vpOutWidth*m_vpOutHeight);
    }

    // Initialize MSDK decoder
    sts = m_mfxDecode->Init(&m_decParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        
    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
        return -1;
    }

    // Initialize MSDK VP
    sts = m_mfxVpp->Init(&m_vppParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

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
        m_mfxBS.DataLength += m_bufferLength;
        copiedLen += m_bufferLength;
        m_bufferLength = 0;
        m_bufferOffset = 0;
    }

    while (m_mfxBS.DataLength < m_mfxBS.MaxLength)
    {
        VADataPacket *packet = AcquireInput();
        if (!packet)
        {
            ERRLOG("Input packet is null!\n");
            break;
        }

        VAData *data = packet->front();
        if (!data)
        {
            ERRLOG("Input packet data is null!\n");
            break;
        }

        uint8_t *src = data->GetSurfacePointer();
        if (!src)
        {
            ERRLOG("Input packet surface is null!\n");
            break;
        }

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

int DecodeThreadBlock::CaptureSurface(mfxFrameSurface1* pSurface, uint8_t *pOutBuffer)
{
    MSDK_CHECK_POINTER(pOutBuffer, MFX_ERR_NULL_PTR);

    mfxStatus sts;
    sts = m_mfxAllocator->Lock(m_mfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

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


    uint8_t *pTemp = pOutBuffer;
    if(m_vpOutFormat == MFX_FOURCC_NV12)
    {
        ptr	= pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;

        for(int i = 0; i < h; i++)
        {
            memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
        }

        ptr	= pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
        pTemp = pOutBuffer + w*h;
        for(int i = 0; i < h / 2; i++)
        {
            memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
        }
    }

    else if(!m_rgbpWA)
    {
        uint8_t *pTemp = pOutBuffer + 2 * w * h;
        ptr   = pData->B + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
        for (int i = 0; i < h; i++)
        {
            memcpy(pTemp  + i*w, ptr + i * pData->Pitch, w);
        }

        ptr	= pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
        pTemp = pOutBuffer + w*h;
        for(int i = 0; i < h; i++)
        {
            memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
        }

        ptr	= pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
        pTemp = pOutBuffer;
        for(int i = 0; i < h; i++)
        {
            memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
        }
    }
    else
    {
        uint8_t *pbuffer = new uint8_t[pData->Pitch*h];
        MSDK_CHECK_POINTER(pbuffer, MFX_ERR_MEMORY_ALLOC);

        memcpy(pbuffer, pData->B, pData->Pitch * h);

        uint8_t *ptrR = pOutBuffer;
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

    sts = m_mfxAllocator->Unlock(m_mfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return 0;
}

int DecodeThreadBlock::DumpVPPOutput(uint8_t *pOutBuffer, FILE* fp_dumpall)
{
    MSDK_CHECK_POINTER(pOutBuffer, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(fp_dumpall, MFX_ERR_NULL_PTR);

    if(m_vpOutFormat == MFX_FOURCC_NV12)
        fwrite(pOutBuffer, 1, m_vpOutWidth * m_vpOutHeight * 3 /2, fp_dumpall);
    else
        fwrite(pOutBuffer, 1, m_vpOutWidth * m_vpOutHeight * 3, fp_dumpall);

    return 0;
}

int DecodeThreadBlock::Loop()
{
    TRACE("");
    int ret = 0;
    mfxStatus sts = MFX_ERR_NONE;
    bool bNeedMore = false;
    mfxSyncPoint syncpDec;
    mfxSyncPoint syncpVPP;
    int nIndexDec = 0;
    int nIndexVpIn = 0;
    int nIndexVpOut = 0;
    uint32_t nDecoded = 0;
    FILE* fp_dumpall = nullptr;
    TRACE("m_vpDumpAllFrame %d", m_vpDumpAllFrame);
    if(m_vpDumpAllFrame){
        std::string filename = m_usersetdumpname;
        if(!filename.empty())
        {
            if(m_channel != 0){
                filename = m_usersetdumpname + std::to_string(m_channel);
            }
        }
        else
        {
            filename = string("VPOut_") + std::to_string(m_channel) + string(".")
                    + std::to_string(m_vpOutWidth) + string("x") + std::to_string(m_vpOutHeight);
            if (m_vpOutFormat == MFX_FOURCC_NV12)
                filename += string(".nv12");
            else
                filename += string(".rgbp");
        }
        TRACE("filename %S", filename);
        fp_dumpall = fopen(filename.c_str(), "wb");
        if (fp_dumpall == NULL)
        {
            ERRLOG("Create dump.yuv failed\n");
        }
        INFO("m_vpOutFormat = %d, m_vpOutWidth = %d, m_vpOutHeight = %d\n",
              m_vpOutFormat, m_vpOutWidth, m_vpOutHeight);
    }


    while ((MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts))
    {
        if (m_stop)
            break;

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

        if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts)
        {
            nIndexDec =GetFreeSurface(m_decodeSurfaces, m_decOutRefs, m_decodeSurfNum);
            while(nIndexDec == MFX_ERR_NOT_FOUND)
            {
                if (m_stop)
                    goto exit;

                TRACE("Cant get decode output buffer");
                usleep(10000);
                nIndexDec = GetFreeSurface(m_decodeSurfaces, m_decOutRefs, m_decodeSurfNum);

                //for (int j = 0; j < m_decodeSurfNum; j++)
                //{
                //    printf("%d, ", m_decOutRefs[j]);
                //}
                //printf("\n");
            }
        }
        TRACE(" get index %d\n", nIndexDec);

        sts = m_mfxDecode->DecodeFrameAsync(&m_mfxBS, m_decodeSurfaces[nIndexDec], &m_vpInSurface, &syncpDec);

        if (sts > MFX_ERR_NONE && syncpDec)
        {
            bNeedMore = false;
            sts = MFX_ERR_NONE;
        }
        else if(MFX_ERR_MORE_DATA == sts)
        {
            bNeedMore = true;
        }
        else if(MFX_ERR_MORE_SURFACE == sts)
        {
            bNeedMore = true;
        }
        if (sts == MFX_ERR_NONE)
        {
            ++ nDecoded;
            Statistics::getInstance().Step(DECODED_FRAMES);

            if(m_vpDumpAllFrame && m_bEnableDecPostProc && !m_bEnableTwoPassesScaling)
            {
                //This needs to be disabled for 2pass designs across VDBox and VEBox
                //Capture is required only to dump. Reuse m_vpOutBuffers for capture

                //NV12 output format is expected here
                assert(m_vpOutFormat == MFX_FOURCC_NV12);
                CaptureSurface(m_vpInSurface, m_vpOutBuffers[nIndexVpOut]);
                DumpVPPOutput(m_vpOutBuffers[nIndexVpOut], fp_dumpall);
            }
        }
        else
        {
            continue;
        }

        int curDecRef = (m_vpRatio && ((nDecoded %m_vpRatio) == 0)) ? m_decodeRefNum : 0;
        if (m_decodeOutputWithVP && m_vpRatio && (nDecoded %m_vpRatio) == 0)
        {
            ++ curDecRef;
        }
        // push decoded output surface to output pin
        VADataPacket *outputPacket = DequeueOutput();
        if (!outputPacket)
        {
            if (m_stop)
                goto exit;
            else
                continue;
        }

        if (curDecRef) // if decoded output is needed
        {
            VAData *vaData = VAData::Create(m_vpInSurface, m_mfxAllocator);
            
            int outputIndex = -1;
            for (int j = 0; j < m_decodeSurfNum; j ++)
            {
                if (m_decodeSurfaces[j] == m_vpInSurface)
                {
                    outputIndex = j;
                    break;
                }
            }
            if (outputIndex == -1)
            {
                ERRLOG("    Error: decode output surface not one of the working surfaces\n");
                vaData->DeRef();
                continue;
            }
            vaData->SetExternalRef(&m_decOutRefs[outputIndex]);
            vaData->SetID(m_channel, nDecoded);
            vaData->SetRef(curDecRef);
            outputPacket->push_back(vaData);
        }
        
        if ((!m_bEnableDecPostProc|| m_bEnableTwoPassesScaling) && m_vpRatio && (nDecoded % m_vpRatio) == 0)
        {
            nIndexVpOut = GetFreeSurface(m_vpOutSurfaces, m_vpOutRefs, m_vpOutSurfNum);
            while(nIndexVpOut == MFX_ERR_NOT_FOUND)
            {
                if (m_stop)
                    goto exit;

                TRACE("Channel %d: Not able to find an avaialbe VPP output surface", m_channel);
                nIndexVpOut = GetFreeSurface(m_vpOutSurfaces, m_vpOutRefs, m_vpOutSurfNum);
            }

            TRACE("VPP get index %d", nIndexVpOut);

            while (1)
            {
                if (m_stop)
                    goto exit;

                sts = m_mfxVpp->RunFrameVPPAsync(m_vpInSurface, m_vpOutSurfaces[nIndexVpOut], nullptr, &syncpVPP);

                if (MFX_ERR_NONE < sts && !syncpVPP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                    {
                        usleep(1000); // wait if device is busy
                    }
                }
                else if (MFX_ERR_NONE < sts && syncpVPP)
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                }
                else{
                    break; // not a warning
                }
            }

            bool enableVppDumps = (!m_bEnableDecPostProc || m_bEnableTwoPassesScaling) &&
                (m_vpRatio && (nDecoded %m_vpRatio) == 0) &&
                (m_vpDumpAllFrame == 1);

            if (sts == MFX_ERR_MORE_DATA)
            {
                continue;
            }
            else if (sts == MFX_ERR_NONE)
            {
                TRACE("VP one frame\n");
                if (m_vpRefNum && !m_vpMemOutTypeVideo)
                {
                    sts = m_mfxSession->SyncOperation(syncpVPP, 60000); // Synchronize. Wait until VPP frame is ready

                    CaptureSurface(m_vpOutSurfaces[nIndexVpOut], m_vpOutBuffers[nIndexVpOut]);
                    if (enableVppDumps)
                        DumpVPPOutput(m_vpOutBuffers[nIndexVpOut], fp_dumpall);

                    int w = m_vpOutSurfaces[nIndexVpOut]->Info.CropW;
                    int h = m_vpOutSurfaces[nIndexVpOut]->Info.CropH;

                    if (w == 0 || h == 0) {
                        w = m_vpOutSurfaces[nIndexVpOut]->Info.Width;
                        h = m_vpOutSurfaces[nIndexVpOut]->Info.Height;
                        }

                    VAData *vaData = VAData::Create(m_vpOutBuffers[nIndexVpOut], w, h, w, m_vpOutFormat);
                    vaData->SetExternalRef(&m_vpOutRefs[nIndexVpOut]); 
                    vaData->SetID(m_channel, nDecoded);
                    vaData->SetRef(m_vpRefNum);
                    outputPacket->push_back(vaData);
                }
                // Transcoding case: DEC->ENC:
                // ENC: required video memory output!
                if ((m_vpMemOutTypeVideo) && (m_vpRefNum))
                {
                    // for now, this sync is needed. the inference engine use vaSyncSurface, but the VP task might
                    // not be submitted when the vaSyncSurface is called. MSDK level sync is needed.
                    sts = m_mfxSession->SyncOperation(syncpVPP, 60000);
                    if (sts != MFX_ERR_NONE)
                    {
                        ERRLOG("Error: SyncOperation on VPP returns %d, terminate the decode thread", sts);
                        ret = -1;
                        goto exit;
                    }

                    if (enableVppDumps) {
                        //Capture is required only to dump
                        CaptureSurface(m_vpOutSurfaces[nIndexVpOut], m_vpOutBuffers[nIndexVpOut]);
                        DumpVPPOutput(m_vpOutBuffers[nIndexVpOut], fp_dumpall);
                    }

                    VAData *vaData = VAData::Create(m_vpOutSurfaces[nIndexVpOut], m_mfxAllocator);
                    // Increasing Ref counter manually
                    m_vpOutRefs[nIndexVpOut]++;
                    vaData->SetExternalRef(&m_vpOutRefs[nIndexVpOut]);
                    //vaData->SetRef(1);
                    vaData->SetID(m_channel, nDecoded);
                    outputPacket->push_back(vaData);
                }
            }
        }
        EnqueueOutput(outputPacket);

        if (m_frameNumber != 0 && nDecoded >= m_frameNumber)
        {
            break;
        }
    }

exit:
    if(m_vpDumpAllFrame && fp_dumpall)
        fclose(fp_dumpall);

    TRACE("DecodeThreadBlock::Loop()    Finished");
    Statistics::getInstance().CountDown();

    return ret;
}

int DecodeThreadBlock::GetFreeSurface(mfxFrameSurface1 **surfaces, int *refs, uint32_t count)
{
    if (surfaces)
    {
        for (uint32_t i = 0; i < count; i ++)
        {
            int refNum = (refs == nullptr)?0:refs[i];
            if (surfaces[i]->Data.Locked == 0 && refNum == 0)
            {
                return i;
            }
        }
    }
    return MFX_ERR_NOT_FOUND;
}
