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

#include "common.h"
#include "DecodeThreadBlock.h"
#include "EncodeThreadBlock.h"
#include "InferenceThreadBlock.h"
#include "ConnectorRR.h"
#include "Statistics.h"

#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>

std::string input_filename;
std::string model_name;
std::string infer_device = "GPU";
std::string model_type = "RCAN";

void App_ShowUsage(void)
{
    printf("Usage: TranscodeSR -i input.264 -m model_file -device [CPU, GPU (default)] -type [SISR, RCAN(default)]\n");
}

void ParseOpt(int argc, char *argv[])
{
    if (argc < 5)
    {
        App_ShowUsage();
        exit(0);
    }

    std::vector <std::string> sources;
    std::string arg = argv[1];
    if ((arg == "-h") || (arg == "--help"))
    {
        App_ShowUsage();
        exit(0);
    }
    for (int i = 1; i < argc; ++i)
        sources.push_back(argv[i]);

    for (int i = 0; i < argc-1; ++i)
    {
        if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        if (sources.at(i) == "-m")
            model_name = sources.at(++i);
        if (sources.at(i) == "-device")
            infer_device = sources.at(++i);
        if (sources.at(i) == "-type")
            model_type = sources.at(++i);
    }

    if (input_filename.empty())
    {
        printf("ERROR: input video file was not specificed\n");
        App_ShowUsage();
        exit(0);
    }
    if (model_name.empty())
    {
        printf("ERROR: model name was not specificed\n");
        App_ShowUsage();
        exit(0);
    }
    if (!(infer_device == "CPU" || infer_device == "GPU"))
    {
        printf("ERROR: Invalid infernce device %s, only [CPU, GPU] are supported\n", infer_device.c_str());
        App_ShowUsage();
        exit(0);
    }
    if (!(model_type == "SISR" || model_type == "RCAN"))
    {
        printf("ERROR: Invalid model type %s, only [SISR, RCAN] are supported\n", infer_device.c_str());
        App_ShowUsage();
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    ParseOpt(argc, argv);

    // default is RCAN
    int vpp_w = 640;
    int vpp_h = 360;
    int out_w = 1280;
    int out_h = 720;
    InferenceModelType infer_type = RCAN;
    if (model_type == "SISR")
    {
        vpp_w = 480;
        vpp_h = 270;
        out_w = 1920;
        out_h = 1080;
        infer_type = SISR;
    }
    std::string model_xml = model_name + ".xml";
    std::string model_bin = model_name + ".bin";;

    std::unique_ptr<DecodeThreadBlock> d = std::make_unique<DecodeThreadBlock>(0);
    std::unique_ptr<InferenceThreadBlock> infer = std::make_unique<InferenceThreadBlock>(0, infer_type);
    std::unique_ptr<EncodeThreadBlock> e = std::make_unique<EncodeThreadBlock>(0, VAEncodeAvc);
    std::unique_ptr<VAFilePin> pin = std::make_unique<VAFilePin>(input_filename.c_str());
    std::unique_ptr<VASinkPin> sink = std::make_unique<VASinkPin>();

    VAConnectorRR *c1 = new VAConnectorRR(1, 1, 10);
    VAConnectorRR *c2 = new VAConnectorRR(1, 1, 10);

    d->ConnectInput(pin.get());
    d->ConnectOutput(c1->NewInputPin());
    d->SetDecodeOutputRef(1); 
    d->SetVPOutputRef(1);
    d->SetVPRatio(1);
    d->SetVPOutResolution(vpp_w, vpp_h);
    d->SetVPOutFormat(MFX_FOURCC_RGBP);
    d->Prepare();
    uint32_t w, h;
    d->GetDecodeResolution(&w, &h);

    infer->ConnectInput(c1->NewOutputPin());
    infer->ConnectOutput(c2->NewInputPin());
    infer->SetDevice(infer_device.c_str());
    infer->SetModelFile(model_xml.c_str(), model_bin.c_str());
    //infer->SetOutputResolution(w*4, h*4);
    infer->Prepare();

    e->ConnectInput(c2->NewOutputPin());
    e->ConnectOutput(sink.get());
    e->SetAsyncDepth(1);
    e->SetInputFormat(MFX_FOURCC_NV12);
    e->SetInputResolution(out_w, out_h);
    e->SetOutputRef(0); // not following blocks will use encoder output
    e->SetEncodeOutDump(true);
    e->Prepare();

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0);

    VAThreadBlock::StopAllThreads();

    delete c1;
    delete c2;
    return 0;
}
