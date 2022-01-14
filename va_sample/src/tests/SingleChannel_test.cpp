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

#include <stdio.h>
#include <string>

#include "DataPacket.h"
#include "ConnectorRR.h"
#include "InferenceThreadBlock.h"
#include "DecodeThreadBlock.h"
#include "DisplayThreadBlock.h"
#include "TrackingThreadBlock.h"
#include "Statistics.h"

std::string input_file = "/home/hefan/workspace/VA/video_analytics_Intel_GPU/test_content/video/0.h264";
int vp_ratio = 1;
bool bDisplay = false;
std::string model_path;

void ParseOpt(int argc, char *argv[])
{
    if (argc < 2)
    {
        exit(0);
    }
    std::vector <std::string> sources;
    std::string arg = argv[1];
    if ((arg == "-h") || (arg == "--help"))
    {
        exit(0);
    }
    for (int i = 1; i < argc; ++i)
        sources.push_back(argv[i]);

    for (int i = 0; i < argc-1; ++i)
    {
        if (sources.at(i) == "-i")
            input_file = sources.at(++i);
        else if (sources.at(i) == "-r")
            vp_ratio = stoi(sources.at(++i));
        else if (sources.at(i) == "-d")
            bDisplay = true;
        else if (sources.at(i) == "-m")
            model_path = sources.at(++i);
    }

    if (input_file.empty())
    {
        printf("Missing input file name!!!!!!!\n");
        exit(0);
    }
    if (model_path.empty())
    {
        model_path = "../../models";
    }
}


int main(int argc, char *argv[])
{
    ParseOpt(argc, argv);

    DecodeThreadBlock *dec =  new DecodeThreadBlock(0);
    InferenceThreadBlock *infer =  new InferenceThreadBlock(0, MOBILENET_SSD_U8);
    TrackingThreadBlock *track = new TrackingThreadBlock(0);
    DisplayThreadBlock *displayBlock = new DisplayThreadBlock();
    VAConnectorRR *c1 = new VAConnectorRR(1, 1, 100);
    VAConnectorRR *c2 = new VAConnectorRR(1, 1, 100);
    VAConnectorRR *c3 = new VAConnectorRR(1, 1, 100);
    VAFilePin *pin = new VAFilePin(input_file.c_str());
    VASinkPin *sink = new VASinkPin();

    dec->ConnectInput(pin);
    dec->ConnectOutput(c1->NewInputPin());
    dec->SetDecodeOutputRef(bDisplay?2:1);
    dec->SetVPOutputRef(1);
    dec->SetVPRatio(vp_ratio);
    dec->SetVPOutResolution(300, 300);
    dec->SetVPOutFormat(MFX_FOURCC_NV12);
    dec->SetVPMemOutTypeVideo(true);
    CHECK_STATUS(dec->Prepare());

    infer->ConnectInput(c1->NewOutputPin());
    infer->ConnectOutput(c2->NewInputPin());
    infer->SetAsyncDepth(1);
    infer->SetBatchNum(1);
    infer->SetOutputRef(2);
    infer->SetDevice("GPU");

    infer->EnabelSharingWithVA();
    char model_file[512], coeff_file[512];
    sprintf(model_file, "%s/mobilenet-ssd.xml", model_path.c_str());
    sprintf(coeff_file, "%s/mobilenet-ssd.bin", model_path.c_str());
    infer->SetModelFile(model_file, coeff_file);
    CHECK_STATUS(infer->Prepare());

    if (vp_ratio > 1)
    {
        track->ConnectInput(c2->NewOutputPin());
        if (bDisplay)
        {
            track->ConnectOutput(c3->NewInputPin());
            displayBlock->ConnectInput(c3->NewOutputPin());
            displayBlock->ConnectOutput(sink);
            CHECK_STATUS(displayBlock->Prepare());
        }
        else
        {
            track->ConnectOutput(sink);
        }
        CHECK_STATUS(track->Prepare());
    }
    else
    {
        if (bDisplay)
        {
            displayBlock->ConnectInput(c2->NewOutputPin());
            displayBlock->ConnectOutput(sink);
            CHECK_STATUS(displayBlock->Prepare());
        }
        else
        {
            infer->ConnectOutput(sink);
        }
    }

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0);

    pause();
    return 0;
}
