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

#ifndef __INFERRENCE_YOLO_H__
#define __INFERRENCE_YOLO_H__

#include "InferenceOV.h"

class InferenceYOLO : public InferenceOV
{
protected:
    class Region {
    public:
        int num = 0;
        int classes = 0;
        int coords = 0;
        std::vector<float> anchors;
        int outputWidth = 0;
        int outputHeight = 0;

        Region(int classes, int coords, const std::vector<float>& anchors, const std::vector<int64_t>& masks, int outputWidth, int outputHeight);
    };
public:
    enum YoloVersion {
        YOLO_V4
    };

    enum YoloInputDim {
        YOLO_IN_BATCH,
        YOLO_IN_CHANNEL,
        YOLO_IN_HEIGHT,
        YOLO_IN_WIDTH
    };

    enum YoloOutputShape {
        YOLO_OUT_BATCH,
        YOLO_OUT_NUM,
        YOLO_OUT_CELLHEIGHT,
        YOLO_OUT_CELLWIDTH
    };

    InferenceYOLO();
    virtual ~InferenceYOLO();

    virtual int Load(const char *device, const char *model, const char *weights);

    virtual void GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc);

protected:
    // derived classes need to fill the dst with the img, based on their own different input dimension
    void CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex);

    // derived classes need to fill VAData by the result, based on their own different output demension
    int Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channels, uint32_t *frames, uint32_t *roiIds);

    void SetDataPorts();

    uint32_t GetInputWidth() {return m_inputWidth; }
    uint32_t GetInputHeight() {return m_inputHeight; }

    // model related
    static int CalculateEntryIndex(int entriesNum, int lcoords, int lclasses, int location, int entry);
    static double IntersectionOverUnion(VAData* vd1, VAData* vd2);
    void ProcessYOLOOutput(const void* result, const Region& region,
        const int sideW, const int sideH, const uint32_t scaledW, const uint32_t scaledH,
        const uint32_t frameId, const uint32_t channelId, std::vector<VAData*>& yolodatas);

    uint32_t m_inputWidth;
    uint32_t m_inputHeight;
    uint32_t m_channelNum;
    std::map<std::string, Region> m_regions;
    double m_boxIOUThreshold;
    bool m_useAdvancedPostProcessing;
    YoloVersion m_yoloVersion;
    const std::vector<float> m_presetAnchors;
    const std::vector<int64_t> m_presetMasks;
};

#endif //__INFERRENCE_YOLO_H__
