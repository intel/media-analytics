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

#include "InferenceYOLO.h"
#include <ie_plugin_config.hpp>
#include <inference_engine.hpp>
#include <logs.h>
#include "DataPacket.h"

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

std::vector<float> defaultAnchors[] = {
    // YOLOv4
    { 12.0f, 16.0f, 19.0f, 36.0f, 40.0f, 28.0f,
      36.0f, 75.0f, 76.0f, 55.0f, 72.0f, 146.0f,
      142.0f, 110.0f, 192.0f, 243.0f, 459.0f, 401.0f}
};

const std::vector<int64_t> defaultMasks[] = {
    // YOLOv4
    {0, 1, 2, 3, 4, 5, 6, 7, 8 }
};

static inline float sigmoid(float x) {
    return 1.f / (1.f + exp(-x));
}

const float defaultYOLOv4BoxIOUThreshold = 0.5;
const bool defaultUseAdvancedPostProcessing = true;

InferenceYOLO::InferenceYOLO():
    m_inputWidth(0),
    m_inputHeight(0),
    m_channelNum(1),
    m_boxIOUThreshold(defaultYOLOv4BoxIOUThreshold),
    m_useAdvancedPostProcessing(defaultUseAdvancedPostProcessing),
    m_yoloVersion(YOLO_V4),
    m_presetAnchors(defaultAnchors[YOLO_V4]),
    m_presetMasks(defaultMasks[YOLO_V4])
{
}

InferenceYOLO::~InferenceYOLO()
{
}

InferenceYOLO::Region::Region(int classes, int coords, const std::vector<float>& anchors, const std::vector<int64_t>& masks, int outputWidth, int outputHeight) :
    classes(classes), coords(coords),
    outputWidth(outputWidth), outputHeight(outputHeight) {
    num = masks.size();

    if (anchors.size() == 0 || anchors.size() % 2 != 0) {
        throw std::runtime_error("Explicitly initialized region should have non-empty even-sized regions vector");
    }

    if (num) {
        this->anchors.resize(num * 2);

        for (int i = 0; i < num; ++i) {
            this->anchors[i * 2] = anchors[masks[i] * 2];
            this->anchors[i * 2 + 1] = anchors[masks[i] * 2 + 1];
        }
    }
    else {
        this->anchors = anchors;
        num = anchors.size() / 2;
    }
}

void InferenceYOLO::SetDataPorts()
{
    TRACE("");
    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    if (inputInfo.begin() == inputInfo.end())
    {
        ERRLOG("inputInfo is empty!\n");
        return;
    }

    auto& inputInfoFirst = inputInfo.begin()->second;
    inputInfoFirst->setPrecision(Precision::U8);
    inputInfoFirst->getInputData()->setLayout(Layout::NCHW);
    if (m_shareSurfaceWithVA)
    {
        inputInfoFirst->getPreProcess().setColorFormat(ColorFormat::NV12);
    }

    // ---------------------------Set outputs ------------------------------------------------------	
    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    for (auto& output : outputInfo) {
        output.second->setPrecision(InferenceEngine::Precision::FP32);
        if (output.second->getDims().size() == 4) {
            output.second->setLayout(InferenceEngine::Layout::NCHW);
        }
    }

}

int InferenceYOLO::Load(const char *device, const char *model, const char *weights)
{
    int ret = InferenceOV::Load(device, model, weights);
    INFO("device %s, model %s, weights %s\n", device, model, weights);
    if (ret)
    {
        ERRLOG(" failed  ret %d \n", ret);
        return ret;
    }

    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    auto& inputInfoFirst = inputInfo.begin()->second;
    const InferenceEngine::SizeVector inputDims = inputInfoFirst->getTensorDesc().getDims();
    m_channelNum = inputDims[YOLO_IN_CHANNEL];
    m_inputWidth = inputDims[YOLO_IN_WIDTH];
    m_inputHeight = inputDims[YOLO_IN_HEIGHT];

    // ---------------------------Set outputs ------------------------------------------------------
    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    int num = 3;
    int i = 0;
    int yolo_class_offset = 5;
    int yolo_num_coords = 4;
    auto chosenMasks = m_presetMasks.size() ? m_presetMasks : defaultMasks[m_yoloVersion];
    if (chosenMasks.size() != num * outputInfo.size())
    {
        ERRLOG("Invalid size of masks array, got %s , should be %s\n",
            std::to_string(m_presetMasks.size()), std::to_string(num * outputInfo.size()));
        return -1;
    }

    std::sort(m_outputsNames.begin(), m_outputsNames.end(),
        [&outputInfo](const std::string& x, const std::string& y)
            {return outputInfo[x]->getDims()[YOLO_OUT_CELLHEIGHT] > outputInfo[y]->getDims()[YOLO_OUT_CELLHEIGHT];});

    for (const auto& name : m_outputsNames)
    {
        auto& output = outputInfo[name];
        auto shape = output->getDims();
        auto classes = shape[YOLO_OUT_NUM] / num - yolo_class_offset;
        if (shape[YOLO_OUT_NUM] % num != 0)
        {
            ERRLOG("The output blob %s has wrong 2nd dimension\n", name);
            return -1;
        }
        m_regions.emplace(name, Region(classes, yolo_num_coords,
            m_presetAnchors.size() ? m_presetAnchors : defaultAnchors[m_yoloVersion],
            std::vector<int64_t>(chosenMasks.begin() + i * num, chosenMasks.begin() + (i + 1) * num),
            shape[YOLO_OUT_CELLWIDTH], shape[YOLO_OUT_CELLHEIGHT]));
        i++;
    }
    return 0;
}

void InferenceYOLO::GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc)
{
    TRACE("");
    *width = m_inputWidth;
    *height = m_inputHeight;
    *fourcc = m_shareSurfaceWithVA?0x3231564e:0x50424752; //MFX_FOURCC_RGBP
}

void InferenceYOLO::CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex)
{
    uint8_t *input = (uint8_t *)dst;
    input += batchIndex * m_inputWidth * m_inputHeight * m_channelNum;
    memcpy(input, img, m_channelNum * m_inputWidth * m_inputHeight);
    TRACE("");
}

int InferenceYOLO::CalculateEntryIndex(int totalCells, int lcoords, int lclasses, int location, int entry)
{
    int n = location / totalCells;
    int loc = location % totalCells;
    return (n * (lcoords + lclasses + 1) + entry) * totalCells + loc;
}


double InferenceYOLO::IntersectionOverUnion(VAData* vd1, VAData* vd2)
{
    float l1, r1, t1, b1;
    float l2, r2, t2, b2;
    vd1->GetRoiRegion(&l1, &t1, &r1, &b1);
    vd2->GetRoiRegion(&l2, &t2, &r2, &b2);
    double overlappingWidth = fmin(r1, r2) - fmax(l1, l2);
    double overlappingHeight = fmin(b1, b2) - fmax(t1, t2);
    double intersectionArea = (overlappingWidth < 0 || overlappingHeight < 0) ? 0 : overlappingHeight * overlappingWidth;
    double unionArea = (r1 - l1) * (b1 - t1) + (r2 - l2) * (b2 - t2) - intersectionArea;
    return intersectionArea / unionArea;
}


void InferenceYOLO::ProcessYOLOOutput(const void* result, const Region& region,
    const int sideW, const int sideH, const uint32_t scaledW,
    const uint32_t scaledH, const uint32_t frameId,
    const uint32_t channelId,
    std::vector<VAData*>& yolodatas)
{
    auto entriesNum = sideW * sideH;
    float* curResult = (float*)result;

    auto postprocessRawData = sigmoid;

    // --------------------------- Parsing YOLO Region output -------------------------------------
    for (int i = 0; i < entriesNum; ++i) {
        int row = i / sideW;
        int col = i % sideW;
        for (int n = 0; n < region.num; ++n) {
            //--- Getting region data from blob
            int obj_index = CalculateEntryIndex(entriesNum, region.coords, region.classes, n * entriesNum + i, region.coords);
            int box_index = CalculateEntryIndex(entriesNum, region.coords, region.classes, n * entriesNum + i, 0);
            float scale = postprocessRawData(curResult[obj_index]);

            //--- Preliminary check for confidence threshold conformance
            if (scale >= m_confidenceThreshold) {
                //--- Calculating scaled region's coordinates
                double x = (col + postprocessRawData(curResult[box_index + 0 * entriesNum])) / sideW;
                double y = (row + postprocessRawData(curResult[box_index + 1 * entriesNum])) / sideH;
                double height = std::exp(curResult[box_index + 3 * entriesNum]) * region.anchors[2 * n + 1] / scaledH;
                double width = std::exp(curResult[box_index + 2 * entriesNum]) * region.anchors[2 * n] / scaledW;

                float l = (float)(x - width / 2);
                float t = (float)(y - height / 2);
                float r = (float)(x + width / 2);
                float b = (float)(y + height / 2);
                if (l < 0.0) l = 0.0;
                if (t < 0.0) t = 0.0;
                if (r > 1.0) r = 1.0;
                if (b > 1.0) b = 1.0;

                for (int j = 0; j < region.classes; ++j) {
                    int class_index = CalculateEntryIndex(entriesNum, region.coords, region.classes, n * entriesNum + i, region.coords + 1 + j);
                    float prob = scale * postprocessRawData(curResult[class_index]);

                    //--- Checking confidence threshold conformance and adding region to the list
                    if (prob >= m_confidenceThreshold) {
                        //use j+1 instead of j: to make the class label starts from 1 instead of 0.
                        VAData* data = VAData::Create(l, t, r, b, j+1, prob);
                        data->SetID(channelId, frameId);
                        data->SetRoiIndex(yolodatas.size());
                        yolodatas.push_back(data);
                    }
                }
            }
        }
    }
}


int InferenceYOLO::Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds, uint32_t *roiIds)
{
    TRACE("");
    std::map<std::string, const float*> * curResults = (std::map<std::string, const float*>*) result;

    for (int b = 0; b < count; b++)
    {
        std::vector<VAData*> vadatas;

        for (const auto& name : m_outputsNames) 
        {
            float* curResult = (float*)curResults->find(name)->second;
            Region region = m_regions.find(name)->second;
            int sideW = m_regions.find(name)->second.outputWidth;
            int sideH = m_regions.find(name)->second.outputHeight;
            curResult += b * sideW * sideH * region.num * (region.coords + region.classes + 1);
            this->ProcessYOLOOutput(curResult, region, sideW, sideH, 
                m_modelInputReshapeWidth, m_modelInputReshapeHeight, frameIds[b],
                channelIds[b],
                vadatas);
        }

        uint32_t roiIndex = 0;
        if (m_useAdvancedPostProcessing) {
            // Advanced postprocessing
            // Checking IOU threshold conformance
            // For every i-th object we're finding all objectss it intersects with, and comparing confidence
            // If i-th object has greater confidence than all others, we include it into result
            for (const auto& vd1 : vadatas) {
                bool isGoodResult = true;
                for (const auto& vd2 : vadatas) {
                    if (vd1->Class() == vd2->Class() && vd1->Confidence() < vd2->Confidence() && IntersectionOverUnion(vd1, vd2) >= m_boxIOUThreshold) {
                        // if vd1 is the same as vd2, condition expression will evaluate to false anyway
                        isGoodResult = false;
                        break;
                    }
                }
                if (isGoodResult) {
                    vd1->SetRoiIndex(roiIndex++);
                    datas.push_back(vd1);
                }
            }
        }
        else {
            // Classic postprocessing
            std::sort(vadatas.begin(), vadatas.end(), [](VAData* vd1, VAData* vd2) { return vd1->Confidence() > vd2->Confidence(); });
            for (size_t i = 0; i < vadatas.size(); ++i) {
                if (vadatas[i]->Confidence() == 0)
                    continue;
                for (size_t j = i + 1; j < vadatas.size(); ++j)
                    if (IntersectionOverUnion(vadatas[i], vadatas[j]) >= m_boxIOUThreshold)
                        vadatas[j]->SetConfidence(0.0);
                vadatas[i]->SetRoiIndex(roiIndex++);
                datas.push_back(vadatas[i]);
            }
        }

    }


    return 0;
}

