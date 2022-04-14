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

#include "InferenceThreadBlock.h"
#include "Inference.h"
#include "logs.h"
#include "Statistics.h"
#include <queue>
#include <map>
#include <iostream>
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <va/va.h>

extern VADisplay *m_va_dpy;
InferenceThreadBlock::InferenceThreadBlock(uint32_t index, InferenceModelType type):
    m_index(index),
    m_type(type),
    m_asyncDepth(1),
    m_streamNum(0),
    m_batchNum(1),
    m_modelInputReshapeWidth(0),
    m_modelInputReshapeHeight(0),
    m_confidenceThreshold(0.8),
    m_outRef(1),
    m_device(nullptr),
    m_infer(nullptr),
    m_lastInferID(0),
    m_enableSharing(false)
{
}

InferenceThreadBlock::~InferenceThreadBlock()
{
    if (m_infer)
    {
        InferenceBlock::Destroy(m_infer);
    }
}

int InferenceThreadBlock::PrepareInternal()
{
    m_infer = InferenceBlock::Create(m_type);
    INFO("InferenceBlock::Create(m_type) %d ", m_type);
    m_infer->Initialize(m_batchNum, m_asyncDepth, m_streamNum, m_confidenceThreshold, m_modelInputReshapeHeight, m_modelInputReshapeWidth);
    TRACE("Initialize m_batchNum %d    m_asyncDepth %d   m_confidenceThreshold %d m_modelInputReshapeHeight %d m_modelInputReshapeWidth %d",
        m_batchNum, m_asyncDepth, m_confidenceThreshold, m_modelInputReshapeHeight, m_modelInputReshapeWidth);

    if (m_enableSharing)
    {
        m_infer->JoinVAContext(m_va_dpy);
        TRACE("enableSharing JoinVAContext()");
    }

    int ret = m_infer->Load(m_device, m_modelFile, m_weightsFile);

    INFO("infer Load()   return %d ", ret);

    if (ret)
    {
        ERRLOG("Load model failed!\n");
        std::cout << "load model failed!\n";
        return -1;
    }

    return 0;
}

bool InferenceThreadBlock::CanBeProcessed(VAData *data)
{
    if (data->Type() != USER_SURFACE && data->Type() != MFX_SURFACE && data->Type() != VA_SURFACE)
    {
        return false;
    }
    uint32_t w, h, p, format;
    data->GetSurfaceInfo(&w, &h, &p, &format);

    uint32_t rw, rh, rf;
    m_infer->GetRequirements(&rw, &rh, &rf);

    if (w == rw && p == rw && h == rh && format == rf)
    {
        return true;
    }

    uint32_t arw, arh;
    arw = (rw + 15)/16*16;
    arh = (rh + 15)/16*16;

    return w == arw  && h == arh && format == rf;
}

stopWatch watch = {};
int InferenceThreadBlock::Loop()
{
    std::queue<uint64_t> hasOutputs;
    std::map<uint64_t, VADataPacket *> recordedPackets;
    bool needInput = true;

    TRACE("m_stop %d", m_stop);

    //startTimer(&watch);
    while (!m_stop)
    {
        TRACE("needInput %d ", needInput);
        if (needInput)
        {
            //printf("HFDebug: try to get input in inference\n");
            //stopTimer(&watch);
            //printf("%fms: Before AcquireInput\n", watch.elapsed);
            VADataPacket *InPacket = AcquireInput();
            //stopTimer(&watch);
            //printf("%fms: After AcquireInput\n", watch.elapsed);
            
            TRACE("get input in inference ");

            if (!InPacket || InPacket->size() == 0){
                if (InPacket)
                    ReleaseInput(InPacket);
                continue;
            }

            // get all the inference inputs
            VADataPacket *tempPacket = new VADataPacket;
            std::vector<VAData *>vpOuts;
            uint32_t channelIndex = 0;
            uint32_t frameIndex = 0;
            for (auto ite = InPacket->begin(); ite != InPacket->end(); ite++)
            {
                VAData *data = *ite;
                channelIndex = data->ChannelIndex();
                frameIndex = data->FrameIndex();;
                if (CanBeProcessed(data))
                {
                    //printf("Get VP out\n");
                    vpOuts.push_back(data);
                    tempPacket->push_back(data);
                }
                else
                {
                    tempPacket->push_back(data);
                }
            }
            ReleaseInput(InPacket);

            TRACE("vpOuts % \n", vpOuts.size());
            // insert the images to inference engine
            if (vpOuts.size() > 0 || m_pendingIDs.size() > 0)
            {
                recordedPackets[ID(channelIndex, frameIndex)] = tempPacket;
            }
            else
            {
                // no need for inference
                // no pending IDs before this
                // enqueue to next block directly
                VADataPacket *outputPacket = DequeueOutput();
                if (!outputPacket)
                {
                    delete tempPacket;
                    goto exit;
                }

                outputPacket->insert(outputPacket->end(), tempPacket->begin(), tempPacket->end());
                delete tempPacket;
                TRACE("sent out id directly %llu\n", ID(channelIndex, frameIndex));

                EnqueueOutput(outputPacket);
                continue;
            }

            if (vpOuts.size() == 0)
            {
                m_pendingIDs[m_lastInferID].push_back(ID(channelIndex, frameIndex));
            }

           for (int i = 0; i < vpOuts.size(); i ++)
            {
                if (vpOuts[i]->Type() == USER_SURFACE)
                {
                    TRACE("USER_SURFACE   vpOuts.size() %d   i %d  ", vpOuts.size(), i);
                    m_infer->InsertImage(vpOuts[i]->GetSurfacePointer(), vpOuts[i]->ChannelIndex(), vpOuts[i]->FrameIndex(), vpOuts[i]->RoiIndex());
                }
                else if (vpOuts[i]->Type() == MFX_SURFACE || vpOuts[i]->Type() == VA_SURFACE)
                {
                    TRACE("VA_SURFACE   vpOuts.size() %d   i %d ", vpOuts.size(), i);
                    //stopTimer(&watch);
                    //printf("%fms: Before InsertImage, channel %d frame %d\n", watch.elapsed, vpOuts[i]->ChannelIndex(), vpOuts[i]->FrameIndex());
                    m_infer->InsertImage(vpOuts[i]->GetVASurface(), vpOuts[i]->ChannelIndex(), vpOuts[i]->FrameIndex(), vpOuts[i]->RoiIndex());
                }
                
                Statistics::getInstance().Step(INFERENCE_FRAMES_RECEIVED);
                if (m_type == MOBILENET_SSD_U8 || m_type == YOLO)
                {
                    Statistics::getInstance().Step(INFERENCE_FRAMES_OD_RECEIVED);
                }
                else if (m_type == RESNET_50)
                {
                    Statistics::getInstance().Step(INFERENCE_FRAMES_OC_RECEIVED);
                }
                m_lastInferID = ID(vpOuts[i]->ChannelIndex(), vpOuts[i]->FrameIndex());
                if (m_pendingIDs.find(m_lastInferID) == m_pendingIDs.end())
                {
                    m_pendingIDs[m_lastInferID] = std::vector<uint64_t>();
                }
            }
        }



        // get all avalible inference output
        std::vector<VAData *> outputs;
        std::vector<uint32_t> channels;
        std::vector<uint32_t> frames;
        uint32_t lastSize = 0;
        bool inferenceFree = false;
        bool isPendingTasks = true;
        bool isAllWorkerBusy = false;
        // get available outputs
        while (1)
        {
            if (m_stop)
                goto exit;

            int ret = m_infer->GetOutput(outputs, channels, frames);
            //stopTimer(&watch);
            //printf("%fms: After GetOutput, output size %d\n", watch.elapsed, outputs.size());
            if (ret < 0)
            {
                inferenceFree = true;
            }
            if (ret == -1)
            {
                isPendingTasks = false;
            }
            if (ret == 2)
            {
                isAllWorkerBusy = true;
            }
            if (outputs.size() == lastSize)
            {
                break;
            }
            else
            {
                for (int i = 0; i < frames.size(); i++)
                {
                    Statistics::getInstance().Step(INFERENCE_FRAMES_PROCESSED);
                    if (m_type == MOBILENET_SSD_U8 || m_type == YOLO)
                    {
                        Statistics::getInstance().Step(INFERENCE_FRAMES_OD_PROCESSED);
                    }
                    else if (m_type == RESNET_50)
                    {
                        Statistics::getInstance().Step(INFERENCE_FRAMES_OC_PROCESSED);
                    }
                }
                lastSize = outputs.size();
            }
        }
        TRACE("lastSize %d    outputs.size  %d ", lastSize,  outputs.size());


        // insert the inference outputs to the packets
        int j = 0;
        for (int i = 0; i < channels.size(); i++)
        {
            VADataPacket *targetPacket = recordedPackets[ID(channels[i], frames[i])];

            if (hasOutputs.size() == 0 || ID(channels[i], frames[i]) != hasOutputs.back())
            {
                hasOutputs.push(ID(channels[i], frames[i]));
            }
            
            while (j < outputs.size()
                    && outputs[j]->ChannelIndex() == channels[i]
                    && outputs[j]->FrameIndex() == frames[i])
            {
                outputs[j]->SetRef(m_outRef);
                targetPacket->push_back(outputs[j]);
                ++j;
            }
        }

        // send the packets to next block
        if (hasOutputs.size() == 0)
        {
            needInput = inferenceFree || (!isAllWorkerBusy);
            continue;
        }
        TRACE("hasOutputs size %d,  infernece free %d ", hasOutputs.size(), inferenceFree);

        uint32_t outputNum = hasOutputs.size() - 1; // can't enqueue last one because it may has following outputs in next GetOutput
        if (inferenceFree && !isPendingTasks)
        {
            ++ outputNum; // no inference task now, so the last one can also be enqueued
        }
        if (outputNum == 0 && !inferenceFree && isAllWorkerBusy)
        {
            // No output but inferenece still working
            // wait longer
            needInput = false;
        }
        else
        {
            needInput = true;
        }

        for (int i = 0; i < outputNum; i++)
        {
            uint64_t id = hasOutputs.front();
            hasOutputs.pop();
            VADataPacket *targetPacket = recordedPackets[id];
            recordedPackets.erase(id);

            VADataPacket *outputPacket = DequeueOutput();
            if (!outputPacket)
                goto exit;

            outputPacket->insert(outputPacket->end(), targetPacket->begin(), targetPacket->end());
            delete targetPacket;
            TRACE("sent out id %llu   outputNum %d  i %d \n", id, outputNum, i);
            EnqueueOutput(outputPacket);
            TRACE("finished EnqueueOutput \n");

            if (m_pendingIDs.find(id) != m_pendingIDs.end())
            {
                std::vector<uint64_t> &pendinglist = m_pendingIDs[id];
                for (int i = 0; i < pendinglist.size(); i++)
                {
                    uint64_t pid = pendinglist[i];
                    VADataPacket *ptargetPacket = recordedPackets[pid];
                    recordedPackets.erase(pid);
                    VADataPacket *poutputPacket = DequeueOutput();
                    if (!poutputPacket)
                        goto exit;

                    poutputPacket->insert(poutputPacket->end(), ptargetPacket->begin(), ptargetPacket->end());
                    delete ptargetPacket;
                    EnqueueOutput(poutputPacket);
                }
                m_pendingIDs.erase(id);
            }
        }
    }

exit:
    return 0;
}
