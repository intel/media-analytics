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

#include "CropThreadBlock.h"
#include "common.h"
#include "logs.h"

extern VADisplay m_va_dpy;

#define CHECK_VASTATUS(va_status,func)                                      \
    if (va_status != VA_STATUS_SUCCESS) {                                     \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        return -1;                                                              \
    }

CropThreadBlock::CropThreadBlock(uint32_t index):
    m_index(index),
    m_vpOutFormat(MFX_FOURCC_RGBP),
    m_vpOutWidth(224),
    m_vpOutHeight(224),
    m_inputWidth(0),
    m_inputHeight(0),
    m_bufferNum(256),
    m_batchSize(1),
    m_contextID(0),
    m_outputVASurfs(nullptr),
    m_outBuffers(nullptr),
    m_outRefs(nullptr),
    m_dumpFlag(false),
    m_vaSyncFlag(false),
    m_vpMemOutTypeVideo(false),
    m_keepAspectRatio(false),
    m_pipeflag(0)
{
    TRACE("");

}

CropThreadBlock::~CropThreadBlock()
{
    TRACE("");
    for (int i = 0; i < m_bufferNum; i++)
    {
        if (m_outputVASurfs && m_outputVASurfs[i] != VA_INVALID_ID)
        {
            vaDestroySurfaces(m_va_dpy, &m_outputVASurfs[i], 1);
        }
        if (m_outBuffers && m_outBuffers[i])
        {
            delete[] m_outBuffers[i];
            m_outBuffers[i] = nullptr;
        }
    }

    if(m_outputVASurfs)
        delete[] m_outputVASurfs;

    if(m_outBuffers)
        delete[] m_outBuffers;

    vaDestroyContext(m_va_dpy, m_contextID);

    for (auto ite = m_dumpFps.begin(); ite != m_dumpFps.end(); ite++)
    {
        if (ite->second)
        {
            fclose(ite->second);
        }
    }
}

int CropThreadBlock::PrepareInternal()
{
    TRACE("");
    if (m_va_dpy == nullptr)
    {
        ERRLOG("Error: Libva not initialized\n");
        return -1;
    }

    m_bufferNum *= m_batchSize;

    m_outputVASurfs = new VASurfaceID[m_bufferNum];
    m_outBuffers = new uint8_t*[m_bufferNum];
    m_outRefs = new int[m_bufferNum];
    memset(m_outBuffers, 0, sizeof(m_outBuffers));
    memset(m_outRefs, 0, sizeof(m_outRefs));
    for (int i = 0; i < m_bufferNum; i++)
    {
        m_outputVASurfs[i] = VA_INVALID_ID;
    }

    VAConfigID  config_id = 0;

    // allocate output surface
    VAStatus vaStatus;
    VASurfaceAttrib    surface_attrib;
    surface_attrib.type =  VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = m_vpOutFormat;
    uint32_t format = VA_RT_FORMAT_YUV420;
    if (m_vpOutFormat == MFX_FOURCC_RGBP)
    {
        format = VA_RT_FORMAT_RGBP;
    }
    for(int i = 0; i < m_bufferNum; i ++)
    {
        vaStatus = vaCreateSurfaces(m_va_dpy,
            format,
            m_vpOutWidth,
            m_vpOutHeight,
            &m_outputVASurfs[i],
            1,
            &surface_attrib,
            1);
    }

    // Create config
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    vaStatus = vaGetConfigAttributes(m_va_dpy,
        VAProfileNone,
        VAEntrypointVideoProc,
        &attrib,
        1);
    CHECK_VASTATUS(vaStatus, "vaGetConfigAttributes");
    vaStatus = vaCreateConfig(m_va_dpy,
                                VAProfileNone,
                                VAEntrypointVideoProc,
                                &attrib,
                                1,
                                &config_id);
    CHECK_VASTATUS(vaStatus, "vaCreateConfig");

    vaStatus = vaCreateContext(m_va_dpy,
                                config_id,
                                m_vpOutWidth,
                                m_vpOutHeight,
                                VA_PROGRESSIVE,
                                nullptr,
                                0,
                                &m_contextID);
    CHECK_VASTATUS(vaStatus, "vaCreateContext");

    // allocate output buffers
    
    for (int i = 0; i < m_bufferNum; i++)
    {
        m_outBuffers[i] = new uint8_t[m_vpOutWidth * m_vpOutHeight * 3];
    }

    return 0;
}

int CropThreadBlock::FindFreeOutput()
{
    TRACE("");
    for (int i = 0; i < m_bufferNum; i ++)
    {
        if (m_outRefs[i] <= 0)
        {
            return i;
        }
    }
    // no free output found
    return -1;
}

int CropThreadBlock::Loop()
{
    // dump the cropping output for test
    TRACE("");
    while(!m_stop)
    {
        VADataPacket *input = AcquireInput();
        VADataPacket *output = DequeueOutput();
        VAData *decodeOutput = nullptr;
        std::vector<VAData *> rois;

        if (!input || !output)
            break;

        if (input->size() == 0)
            break;

        for (auto ite = input->begin(); ite != input->end(); ite++)
        {
            VAData *data = *ite;
            if (IsDecodeOutput(data))
            {
                decodeOutput = data;
            }
            else if (IsRoiRegion(data))
            {
                rois.push_back(data);
            }
            else
            {
                output->push_back(data);
            }
        }

        ReleaseInput(input);

        uint32_t decodeWidth = 0, decodeHeight = 0, p, f;
        if (m_inputWidth != 0 && m_inputHeight != 0)
        {
            decodeWidth = m_inputWidth;
            decodeHeight = m_inputHeight;
        }
        else if (decodeOutput)
        {
            decodeOutput->GetSurfaceInfo(&decodeWidth, &decodeHeight, &p, &f);
        }

        if (!decodeOutput || decodeWidth == 0 || decodeHeight == 0)
            continue;

        //printf("in Crop, %d rois\n", rois.size());
        for (int i = 0; i < rois.size(); i++)
        {
            VAData *roi = rois[i];
            float l, r, t, b;
            roi->GetRoiRegion(&l, &t, &r, &b);

            int index = -1;
            while (index == -1)
            {
                if (m_stop)
                    goto exit;

                index = FindFreeOutput();
                if (index == -1)
                    usleep(10000);
            }

            mfxFrameSurface1 *mfxSurf = decodeOutput->GetMfxSurface();
            if (!mfxSurf)
            {
                ERRLOG("Fail to get MSDK surface!\n");
                break;
            }
            VASurfaceID inputSurf;
#ifndef MSDK_2_0_API
            mfxFrameAllocator *alloc = decodeOutput->GetMfxAllocator();
            if (!alloc)
            {
                ERRLOG("Fail to get MSDK allocator!\n");
                break;
            }
            // Get Input VASurface
            mfxHDL handle;
            alloc->GetHDL(alloc->pthis, mfxSurf->Data.MemId, &(handle));
            inputSurf = *(VASurfaceID *)handle;
#else
            mfxHDL handle;
            mfxResourceType type;
            mfxSurf->FrameInterface->GetNativeHandle(mfxSurf, &handle, &type);
            inputSurf = (VASurfaceID)(uint64_t)handle;
#endif
            
            Crop(inputSurf,
                 m_outputVASurfs[index],
                 (uint32_t)(l * decodeWidth),
                 (uint32_t)(t * decodeHeight),
                 (uint32_t)((r - l) * decodeWidth),
                 (uint32_t)((b - t) * decodeHeight),
                  m_keepAspectRatio);

            VAData *cropOut = nullptr;
            if (!m_vpMemOutTypeVideo || m_dumpFlag)
            {
                // copy to system memory if needed
                VAImage surface_image;
                void *surface_p = nullptr;
                VAStatus va_status = vaDeriveImage(m_va_dpy, m_outputVASurfs[index], &surface_image);
                CHECK_VASTATUS(va_status, "vaDeriveImage");
                va_status = vaMapBuffer(m_va_dpy, surface_image.buf, &surface_p);
                CHECK_VASTATUS(va_status, "vaMapBuffer");
                
                uint8_t *y_dst = m_outBuffers[index];
                uint8_t *y_src = (uint8_t *)surface_p;

                if (m_vpOutFormat == MFX_FOURCC_NV12)
                {
                    y_src = (uint8_t *)surface_p + surface_image.offsets[0];
                    for (int row = 0; row < surface_image.height; row++)
                    {
                        memcpy(y_dst, y_src, surface_image.width);
                        y_src += surface_image.pitches[0];
                        y_dst += surface_image.width;
                    }
                    y_src = (uint8_t *)surface_p + surface_image.offsets[1];
                    for (int row = 0; row < surface_image.height/2; row++)
                    {
                        memcpy(y_dst, y_src, surface_image.width);
                        y_src += surface_image.pitches[1];
                        y_dst += surface_image.width;
                    }
                }
                else
                {
                    for (int i = surface_image.num_planes - 1 ; i >= 0; i --)
                    {
                        y_src = (uint8_t *)surface_p + surface_image.offsets[i];
                        for (int row = 0; row < surface_image.height; row++)
                        {
                            memcpy(y_dst, y_src, surface_image.width);
                            y_src += surface_image.pitches[i];
                            y_dst += surface_image.width;
                        }
                    }
                }
                
                vaUnmapBuffer(m_va_dpy, surface_image.buf);
                vaDestroyImage(m_va_dpy, surface_image.image_id);
            }
            if (m_vpMemOutTypeVideo)
            {
                cropOut = VAData::Create(m_outputVASurfs[index], m_vpOutWidth, m_vpOutHeight, m_vpOutWidth, m_vpOutFormat);
            }
            else
            {
                cropOut = VAData::Create(m_outBuffers[index], m_vpOutWidth, m_vpOutHeight, m_vpOutWidth, m_vpOutFormat);
            }
            if (cropOut)
            {
                cropOut->SetID(roi->ChannelIndex(), roi->FrameIndex());
                cropOut->SetRoiIndex(roi->RoiIndex());
                cropOut->SetExternalRef(&m_outRefs[index]);
                cropOut->SetRef(1);
                roi->DeRef(output);
                output->push_back(cropOut);
            }

            if (m_dumpFlag)
            {
                FILE *fp = GetDumpFile(roi->ChannelIndex());
                if (m_vpOutFormat == MFX_FOURCC_NV12)
                {
                    fwrite(m_outBuffers[index], 1, m_vpOutWidth*m_vpOutHeight*3/2, fp);
                }
                else
                {
                    fwrite(m_outBuffers[index], 1, m_vpOutWidth*m_vpOutHeight*3, fp);
                }
            }
        }

        decodeOutput->DeRef(output);
        EnqueueOutput(output);
    }

exit:
    return 0;
}

int CropThreadBlock::Crop(VASurfaceID inSurf,
             VASurfaceID outSurf,
             uint32_t srcx,
             uint32_t srcy,
             uint32_t srcw,
             uint32_t srch,
             bool keepRatio)
{
    TRACE("");
    // prepare VP parameters
    VAStatus va_status;
    VAProcPipelineParameterBuffer pipeline_param;
    VARectangle surface_region, output_region;
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VABufferID filter_param_buf_id = VA_INVALID_ID;

    uint dstw, dsth;
    if (keepRatio)
    {
        double ratio = (double)srcw/(double)srch;
        double ratio2 = (double)m_vpOutWidth/(double)m_vpOutHeight;
        if (ratio > ratio2)
        {
            dstw = m_vpOutWidth;
            dsth = (uint32_t)((double)dstw/ratio);
        }
        else
        {
            dsth = m_vpOutHeight;
            dstw = (uint32_t)((double)dsth * ratio);
        }
    }
    else
    {
        dstw = m_vpOutWidth;
        dsth = m_vpOutHeight;
    }
    surface_region.x = srcx;
    surface_region.y = srcy;
    surface_region.width = srcw;
    surface_region.height = srch;
    output_region.x = 0;
    output_region.y = 0;
    output_region.width = dstw;
    output_region.height = dsth;
    memset(&pipeline_param, 0, sizeof(pipeline_param));
    pipeline_param.surface = inSurf;
    pipeline_param.surface_region = &surface_region;
    pipeline_param.output_region = &output_region;
    pipeline_param.output_background_color = 0xff000000;
    pipeline_param.filter_flags = 0;
    pipeline_param.pipeline_flags = m_pipeflag;
    pipeline_param.filters      = &filter_param_buf_id;
    pipeline_param.num_filters  = 0;

    va_status = vaCreateBuffer(m_va_dpy,
                               m_contextID,
                               VAProcPipelineParameterBufferType,
                               sizeof(pipeline_param),
                               1,
                               &pipeline_param,
                               &pipeline_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaBeginPicture(m_va_dpy,
                               m_contextID,
                               outSurf);
    CHECK_VASTATUS(va_status, "vaBeginPicture");

    va_status = vaRenderPicture(m_va_dpy,
                                m_contextID,
                                &pipeline_param_buf_id,
                                1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    va_status = vaEndPicture(m_va_dpy, m_contextID);
    CHECK_VASTATUS(va_status, "vaEndPicture");

    if (m_vaSyncFlag)
    {
        va_status = vaSyncSurface(m_va_dpy, outSurf);
        CHECK_VASTATUS(va_status, "vaSyncSurface");
    }

    if (filter_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(m_va_dpy,filter_param_buf_id);
    if (pipeline_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(m_va_dpy,pipeline_param_buf_id);

    return 0;
}

bool CropThreadBlock::IsDecodeOutput(VAData *data)
{
    if (data->Type() != MFX_SURFACE)
    {
        return false;
    }
    uint32_t w, h, p, format;
    data->GetSurfaceInfo(&w, &h, &p, &format);

    if (w == m_inputWidth && h == m_inputHeight )
    {
        return true;
    }

    uint32_t arw, arh;
    arw = (m_inputWidth + 15)/16*16;
    arh = (m_inputHeight + 15)/16*16;

    return w == arw  && h == arh;
}

bool CropThreadBlock::IsRoiRegion(VAData *data)
{
    return data->Type() == ROI_REGION;
}

FILE *CropThreadBlock::GetDumpFile(uint32_t channel)
{
    TRACE("");
    auto ite = m_dumpFps.find(channel);
    if (ite == m_dumpFps.end())
    {
        char filename[256];
        FILE *fp = nullptr;
        if (m_vpOutFormat == MFX_FOURCC_NV12)
        {
            sprintf(filename, "CropOut_%d.%dx%d.nv12", channel, m_vpOutWidth, m_vpOutHeight);
            fp = fopen(filename, "wb");
        }
        else
        {
            sprintf(filename, "CropOut_%d.%dx%d.rgbp", channel, m_vpOutWidth, m_vpOutHeight);
            fp = fopen(filename, "wb");
        }
        m_dumpFps[channel] = fp;
    }

    return m_dumpFps[channel];
}

