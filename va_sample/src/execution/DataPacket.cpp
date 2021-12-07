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

#include "DataPacket.h"
#include <unistd.h>

static int initialized = VADataCleaner::getInstance().Initialize(false);

static void *VaDataCleanerFunc(void *arg)
{
    VADataCleaner *cleaner = static_cast<VADataCleaner *>(arg);
    while (cleaner->Continue())
    {
        cleaner->Destroy();
        usleep(100000);
    }
    return (void *)0;
}

VADataCleaner::VADataCleaner():
    m_continue(false),
    m_debug(false)
{
}

VADataCleaner::~VADataCleaner()
{
    Destroy();
    m_continue = false;
    pthread_join(m_threadId, nullptr);
}

int VADataCleaner::Initialize(bool debug)
{
    m_continue = true;
    m_debug = debug;
    pthread_create(&m_threadId, nullptr, VaDataCleanerFunc, (void *)this);
    return 0;
}

void VADataCleaner::Destroy()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_destroyList.empty())
    {
        VAData *data = m_destroyList.front();
        if (m_debug)
        {
            printf("VADataCleaner Thread: delete data %p, channel %d, frame %d\n", data, data->ChannelIndex(), data->FrameIndex());
        }
        delete data;
        m_destroyList.pop_front();
    }
}

void VADataCleaner::Add(VAData *data)
{
    data->Destroy();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_destroyList.push_back(data);
}

VAData::VAData():
    m_mfxSurface(nullptr),
#ifndef MSDK_2_0_API
    m_mfxAllocator(nullptr),
#endif
    m_data(nullptr),
    m_width(0),
    m_height(0),
    m_pitch(0),
    m_fourcc(0),
    m_left(0.0),
    m_right(0.0),
    m_top(0.0),
    m_bottom(0.0),
    m_class(-1),
    m_confidence(1.0),
    m_vaSurf(VA_INVALID_ID),
    m_internalRef(1),
    m_channelIndex(0),
    m_frameIndex(0),
    m_roiIndex(0)
{
    m_ref = &m_internalRef;
}

#ifndef MSDK_2_0_API
VAData::VAData(mfxFrameSurface1 *surface, mfxFrameAllocator *allocator):
    VAData()
{
    m_type = MFX_SURFACE;

    m_mfxSurface = surface;
    m_mfxAllocator = allocator;
    m_width = surface->Info.Width;
    m_height = surface->Info.Height;
    m_pitch = m_width;
    m_fourcc = surface->Info.FourCC;
}
#else
VAData::VAData(mfxFrameSurface1 *surface):
    VAData()
{
    m_type = MFX_SURFACE;

    m_mfxSurface = surface;
    m_width = surface->Info.Width;
    m_height = surface->Info.Height;
    m_pitch = m_width;
    m_fourcc = surface->Info.FourCC;
}

#endif

VAData::VAData(VASurfaceID surface, uint32_t w, uint32_t h, uint32_t p, uint32_t fourcc):
    VAData()
{
    m_type = VA_SURFACE;

    m_vaSurf = surface;
    m_width = w;
    m_height = h;
    m_pitch = p;
    m_fourcc = fourcc;
}


VAData::VAData(uint8_t *data, uint32_t w, uint32_t h, uint32_t p, uint32_t fourcc):
    VAData()
{
    m_type = USER_SURFACE;

    m_data = data;
    m_width = w;
    m_height = h;
    m_pitch = p;
    m_fourcc = fourcc;
}

VAData::VAData(float left, float top, float right, float bottom, int c, float conf):
    VAData()
{
    m_type = ROI_REGION;

    m_left = left;
    m_top = top;
    m_right = right;
    m_bottom = bottom;

    m_class = c;
    m_confidence = conf;
}

VAData::VAData(uint8_t *data, uint32_t offset, uint32_t length):
    VAData()
{
    m_type = USER_BUFFER;

    m_data = data;
    m_offset = offset;
    m_length = length;
}

VAData::VAData(int c, float conf):
    VAData()
{
    m_type = IMAGENET_CLASS;

    m_class = c;
    m_confidence = conf;
}

VAData::~VAData()
{
}

mfxFrameSurface1 *VAData::GetMfxSurface()
{
    if (m_type != MFX_SURFACE)
    {
        return nullptr;
    }
    else
    {
        return m_mfxSurface;
    }
}

#ifndef MSDK_2_0_API
mfxFrameAllocator *VAData::GetMfxAllocator()
{
    if (m_type != MFX_SURFACE)
    {
        return nullptr;
    }
    else
    {
        return m_mfxAllocator;
    }

}
#endif

uint8_t *VAData::GetSurfacePointer()
{
    if (m_type == USER_SURFACE || m_type == USER_BUFFER)
    {
        return m_data;
    }
    else if (m_type == MFX_SURFACE)
    {
        if (m_data == nullptr)
        {
#ifndef MSDK_2_0_API
            m_mfxAllocator->Lock(m_mfxAllocator->pthis, m_mfxSurface->Data.MemId, &(m_mfxSurface->Data));
#else
            m_mfxSurface->FrameInterface->Map(m_mfxSurface, MFX_MAP_READ_WRITE);
#endif
            m_data = m_mfxSurface->Data.Y;
        }
        return m_data;
    }
    else
    {
        return nullptr;
    }
}

VASurfaceID VAData::GetVASurface()
{
    if (m_type == MFX_SURFACE)
    {
        mfxHDL handle;
#ifndef MSDK_2_0_API
        m_mfxAllocator->GetHDL(m_mfxAllocator->pthis, m_mfxSurface->Data.MemId, &(handle));
        return *(VASurfaceID *)handle;
#else
        mfxResourceType type;
        m_mfxSurface->FrameInterface->GetNativeHandle(m_mfxSurface, &handle, &type);
        return (VASurfaceID)(uint64_t)handle;
#endif
    }
    else if (m_type == VA_SURFACE)
    {
        return m_vaSurf;
    }
    else
    {
        return VA_INVALID_ID;
    }
}

void VAData::SetRef(uint32_t count)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    *m_ref = count;
}

void VAData::DeRef(VADataPacket *packet, uint32_t count)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    *m_ref = *m_ref - count;
    if (*m_ref <= 0)
    {
        VADataCleaner::getInstance().Add(this);
    }
    else if (packet)
    {
        packet->push_back(this);
    }
}

void VAData::Destroy()
{
    if (m_type == MFX_SURFACE && m_data != nullptr)
    {
#ifndef MSDK_2_0_API
        m_mfxAllocator->Unlock(m_mfxAllocator->pthis, m_mfxSurface->Data.MemId, &(m_mfxSurface->Data));
#else
        m_mfxSurface->FrameInterface->Unmap(m_mfxSurface);
#endif
    }
#ifdef MSDK_2_0_API
    if (m_type == MFX_SURFACE)
    {
        m_mfxSurface->FrameInterface->Release(m_mfxSurface);
    }
#endif

}


void VAData::GetSurfaceInfo(uint32_t *w, uint32_t *h, uint32_t *p, uint32_t *fourcc)
{
    *w = m_width;
    *h = m_height;
    *p = m_pitch;
    *fourcc = m_fourcc;
}

void VAData::GetRoiRegion(float *left, float *top, float *right, float *bottom)
{
    *left = m_left;
    *right = m_right;
    *top = m_top;
    *bottom = m_bottom;
}

void VAData::GetBufferInfo(uint32_t *offset, uint32_t *length)
{
    *offset = m_offset;
    *length = m_length;
}
