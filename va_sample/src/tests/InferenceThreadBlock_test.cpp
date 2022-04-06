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
#include "Statistics.h"

std::string input_filename;
std::string model_path;
static int duration = -1;
static int batch_num = 1;
static InferenceModelType modelType = MOBILENET_SSD_U8;
static int perf_test = false;

void App_ShowUsage(void)
{
    printf("Usage: multichannel_decode -i input.rgbp [options]\n");
    printf("           -t test duration \n");
    printf("           -b batch number \n");
    printf("           -ssd: running mobilenetSSD\n");
    printf("           -resnet: running Resnet50\n");    
    printf("           -p: run in performance mode\n");
}


void ParseOpt(int argc, char *argv[])
{
    if (argc < 2)
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
        else if (sources.at(i) == "-t")
            duration = stoi(sources.at(++i));
        else if (sources.at(i) == "-b")
            batch_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-ssd")
            modelType = MOBILENET_SSD_U8;
        else if (sources.at(i) == "-resnet")
            modelType = RESNET_50;
        else if (sources.at(i) == "-p")
            perf_test = true;
        else if (sources.at(i) == "-m")
            model_path = sources.at(++i);
    }

    if (input_filename.empty())
    {
        printf("Missing input file name!!!!!!!\n");
        App_ShowUsage();
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

    uint32_t width = 0;
    uint32_t height = 0;

    if (modelType == MOBILENET_SSD_U8)
    {
        width = 300;
        height = 300;
    }
    else if (modelType == RESNET_50)
    {
        width = 224;
        height = 224;
    }
    else if (modelType == SISR)
    {
        width = 480;
        height = 270;
    }
    else if (modelType == RCAN)
    {
        width = 640;
        height = 360;
    }
    else
    {
        App_ShowUsage();
        return -1;
    }

    std::unique_ptr<VARawFilePin> fpin = std::make_unique<VARawFilePin>(input_filename.c_str(), MFX_FOURCC_RGBP, width, height, perf_test);
    std::unique_ptr<VACsvWriterPin> sink;
    if (!perf_test)
        sink.reset(new VACsvWriterPin("output.csv"));
    std::unique_ptr<VASinkPin> emptySink = std::make_unique<VASinkPin>();

    std::unique_ptr<InferenceThreadBlock> infer = std::make_unique<InferenceThreadBlock>(0, modelType);

    infer->ConnectInput(fpin.get());
    if (perf_test)
    {
        infer->ConnectOutput(emptySink.get());
    }
    else
    {
        infer->ConnectOutput(sink.get());
    }
    
    infer->SetAsyncDepth(1);
    infer->SetBatchNum(batch_num);
    infer->SetDevice("GPU");

    char model_file[512], coeff_file[512];
    if (modelType == MOBILENET_SSD_U8)
    {
        sprintf(model_file, "%s/mobilenet-ssd.xml", model_path.c_str());
        sprintf(coeff_file, "%s/mobilenet-ssd.bin", model_path.c_str());
        infer->SetModelFile(model_file, coeff_file);
    }
    else if (modelType == RESNET_50)
    {
        sprintf(model_file, "%s/resnet-50-128.xml", model_path.c_str());
        sprintf(coeff_file, "%s/resnet-50-128.bin", model_path.c_str());
        infer->SetModelFile(model_file, coeff_file);
    }
    else if (modelType == SISR)
    {
        char* model_xml = (char*)"/home/fresh/data/model/single-image-super-resolution-1032.xml";
        char* model_bin = (char*)"/home/fresh/data/model/single-image-super-resolution-1032.bin";
        sprintf(model_file, "%s", model_xml);
        sprintf(coeff_file, "%s", model_bin);
        infer->SetModelFile(model_file, coeff_file);
    }
    else if (modelType == RCAN)
    {
        char* model_xml = (char*)"/home/fresh/data/model/rcan/rcan_360x640_scale2_resgroups5_resblocks8_feats64.xml";
        char* model_bin = (char*)"/home/fresh/data/model/rcan/rcan_360x640_scale2_resgroups5_resblocks8_feats64.bin";
        sprintf(model_file, "%s", model_xml);
        sprintf(coeff_file, "%s", model_bin);
        infer->SetModelFile(model_file, coeff_file);
    }

    //if (channel_num == 1)
    //{
    //    infer->EnabelSharingWithVA();
    //}
    
    CHECK_STATUS(infer->Prepare());

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0, duration);

    VAThreadBlock::StopAllThreads();

    return 0;
}
