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

#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>

#include "ThreadBlock.h"
#include "Inference.h"

class InferenceBlock;

class InferenceThreadBlock : public VAThreadBlock
{
public:
    InferenceThreadBlock(uint32_t index, InferenceModelType type);
    ~InferenceThreadBlock();

    InferenceThreadBlock(const InferenceThreadBlock&) = delete;
    InferenceThreadBlock& operator=(const InferenceThreadBlock&) = delete;

    inline void SetAsyncDepth(uint32_t depth) {m_asyncDepth = depth; }
    inline void SetStreamNum(uint32_t num) {m_streamNum = num; }
    inline void SetBatchNum(uint32_t batch) {m_batchNum = batch; }
    inline void SetModelInputReshapeWidth(uint32_t width) {m_modelInputReshapeWidth = width; }
    inline void SetModelInputReshapeHeight(uint32_t height) {m_modelInputReshapeHeight = height; }
    inline void SetConfidenceThreshold(float confidence) { m_confidenceThreshold = confidence; }
    inline void SetDevice(const char *device) {m_device = device; }
    inline void SetModelFile(const char *model, const char *weights)
    {
        m_modelFile = model;
        m_weightsFile = weights;
    }
    inline void SetOutputRef(int ref) {m_outRef = ref; }

    inline void EnabelSharingWithVA() {m_enableSharing = true; }

    int Loop();

protected:
    int PrepareInternal() override;

    bool CanBeProcessed(VAData *data);

    uint32_t m_index;
    InferenceModelType m_type;
    uint32_t m_asyncDepth;
    uint32_t m_streamNum;
    uint32_t m_batchNum;
    uint32_t m_modelInputReshapeWidth;
    uint32_t m_modelInputReshapeHeight;
    float m_confidenceThreshold;
    int m_outRef;
    const char *m_device;
    const char *m_modelFile;
    const char *m_weightsFile;
    InferenceBlock *m_infer;

    // A, B, C, D, E
    // insert A for inference
    // B, C, D pass the inference
    // then B, C, D are pending IDs of A, because B, C, D can only be sent to next block after A is ready
    std::map<uint64_t, std::vector<uint64_t>> m_pendingIDs;
    uint64_t m_lastInferID;

    bool m_enableSharing;
};