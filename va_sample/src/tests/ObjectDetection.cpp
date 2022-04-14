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
#include "logs.h"

enum eSCALE_mode
{
    eSCALE_RCS = 0,
    eSCALE_VCS,
    eSCALE_VECS,
    eSCALE_Max
};

enum eDETECT_type
{
    eSSD = 0,
    eYOLO
};

static int vp_ratio = 1;
static int channel_num = 1;
static int channel_num_infer = 1;
static int batch_num = 1;
static bool dump_results = false;
static int duration = -1;
static int num_request = 1;
static int num_stream = 0;
std::string input_filename;
std::string model_name;
static eDETECT_type detect_type = eSSD;
static bool perf_test = false;
static bool va_share = false;
static float dconf_threshold = 0.8;
static mfxU32 codec_type = MFX_CODEC_AVC;
static eSCALE_mode scale_mode = eSCALE_VECS;

static const char* default_model_name = "/opt/intel/samples/models/ssd_mobilenet_v1_coco_INT8/ssd_mobilenet_v1_coco";
static const char* default_detect_type = "ssd";
static const float default_dconf_threshold = 0.8;
static const uint32_t default_ssd_input_width = 300;
static const uint32_t default_ssd_input_height = 300;
static const uint32_t default_yolo_input_reshape_width = 416;
static const uint32_t default_yolo_input_reshape_height = 416;
static uint32_t dshape_width = 0;
static uint32_t dshape_height = 0;

void App_ShowUsage(void)
{
    printf("Usage: ObjectDetection -i input.264 [options]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help             Print this help\n");
    printf("  -codec 264|265         Use this codec to decode the video (default: 264)\n");
    printf("  -c dec_channels        Number of decoding channels (default: 1)\n");
    printf("  -m_detect model        xml model file name with absolute path, no .xml needed\n");
    printf("                           default: %s\n", default_model_name);
    printf("  -detect_type ssd|yolo  Detection type\n");
    printf("                           default: %s\n", default_detect_type);
    printf("  -dshape_w width        Detection model input reshape width\n");
    printf("                           ssd default: %d\n", default_ssd_input_width);
    printf("                           yolo default: %d\n", default_yolo_input_reshape_width);
    printf("  -dshape_h height       Detection model input reshape height\n");
    printf("                           ssd default: %d\n", default_ssd_input_height);
    printf("                           yolo default: %d\n", default_yolo_input_reshape_height);
    printf("  -dconf threshold       Minimum detection output confidence, range [0-1] (default: %.1f)\n", default_dconf_threshold);
    printf("  -b batch_number        Batch number in the inference model (default: 1)\n");
    printf("  -infer infer_channels  Number of inference channels (default: 1)\n");
    printf("  -nireq req_number      Set the inference request number (default: 1)\n");
    printf("  -nstreams stream_num   Set the inference stream number (default: 0)\n");
    printf("  -r vp_ratio            Ratio of decoded frames to vp frames (default: 1)\n");
    printf("                           -r 2 means doing vp every other frame\n");
    printf("  -scale hq|fast_inplace|fast\n");
    printf("    hq           - run scaling on EUs thru rcs or ccs depending on the platform\n");
    printf("    fast_inplace - run scaling with affinity to decoding, i.e. on the same vcs; scaling is done thru SFC\n");
    printf("    fast         - run scaling via vecs, scaling is done thru SFC (this is default)\n");
    printf("  -t seconds             How many seconds this app should run\n");
    printf("  -d                     Dump the inference input, raw data of decode and vp\n");
    printf("  -p                     Performance mode, don't dump the csv results\n");
    printf("  -va_share              Share surface between media and inferece\n");
    printf("  -show                  Display every frame\n");
}

void ParseOpt(int argc, char *argv[])
{
    TRACE("input argc %d", argc)
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

    for (int i = 0; i < argc - 1; ++i)
    {
        if (sources.at(i) == "-c")
            channel_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-infer")
            channel_num_infer = stoi(sources.at(++i));
        else if (sources.at(i) == "-b")
            batch_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        else if (sources.at(i) == "-r")
            vp_ratio = stoi(sources.at(++i));
        else if (sources.at(i) == "-t")
            duration = stoi(sources.at(++i));
        else if (sources.at(i) == "-d")
            dump_results = true;
        else if (sources.at(i) == "-p")
            perf_test = true;
        else if (sources.at(i) == "-m_detect")
            model_name = sources.at(++i);
        else if (sources.at(i) == "-detect_type")
        {
            std::string type = sources.at(++i);
            if (type == "ssd")
            {
                detect_type = eSSD;
            }
            else// if (type == "yolo")
            {
                detect_type = eYOLO;
            }
        }
        else if (sources.at(i) == "-dshape_w")
        {
            dshape_width = stoul(sources.at(++i));
        }
        else if (sources.at(i) == "-dshape_h")
        {
            dshape_height = stoul(sources.at(++i));
        }
        else if (sources.at(i) == "-dconf")
        {
            dconf_threshold = stof(sources.at(++i));
        }
        else if (sources.at(i) == "-va_share")
            va_share = true;
        else if (sources.at(i) == "-scale")
        {
            std::string engine = sources.at(++i);
            if (engine == "hq")
            {
                scale_mode = eSCALE_RCS;
            }
            else if (engine == "fast_inplace")
            {
                scale_mode = eSCALE_VCS;
            }
            else if (engine == "fast")
            {
                scale_mode = eSCALE_VECS;
            }
        }
        else if (sources.at(i) == "-codec")
        {
            std::string codec_format = sources.at(++i);
            if (codec_format == "264")
                codec_type = MFX_CODEC_AVC;
            else if (codec_format == "265")
                codec_type = MFX_CODEC_HEVC;
            else
                printf("unknown codec type\n");
        }
        else if (sources.at(i) == "-nireq")
        {
            num_request = stoi(sources.at(++i));
        }
        else if (sources.at(i) == "-nstreams")
        {
            num_stream = stoi(sources.at(++i));
        }
        else
        {
            printf("unknown argument: %s\n", sources.at(i).c_str());
            App_ShowUsage();
            exit(0);
        }
    }

    if (perf_test)
        dump_results = false;

    if (input_filename.empty())
    {
        printf("Missing input file name!!!!!!!\n");
        App_ShowUsage();
        exit(0);
    }
    if (model_name.empty())
    {
        model_name = default_model_name;
    }
    if (dshape_width == 0 || dshape_height == 0)
    {
        if (eSSD == detect_type)
        {
            dshape_width = default_ssd_input_width;
            dshape_height = default_ssd_input_height;
        }
        else if (eYOLO == detect_type)
        {
            dshape_width = default_yolo_input_reshape_width;
            dshape_height = default_yolo_input_reshape_height;
        }
    }
}

int main(int argc, char *argv[])
{
    loglevel_setup();

    ParseOpt(argc, argv);
    std::vector<std::unique_ptr<DecodeThreadBlock>> decodeBlocks;
    std::vector<std::unique_ptr<VAFilePin>> filePins;

    std::unique_ptr<VAConnectorRR> c1 = std::make_unique<VAConnectorRR>(channel_num, channel_num_infer, batch_num * channel_num_infer * 10);
    for (int i = 0; i < channel_num; i++)
    {
        INFO("intput file is %s\n", input_filename.c_str());

        decodeBlocks.push_back(std::make_unique<DecodeThreadBlock>(i));
        filePins.push_back(std::make_unique<VAFilePin>(input_filename.c_str(), perf_test));

        auto& dec = decodeBlocks[i];
        auto& pin = filePins[i];

        dec->ConnectInput(pin.get());
        dec->ConnectOutput(c1->NewInputPin());
        dec->SetDecodeOutputRef(0); // don't need decode output if no display
        dec->SetVPRatio(1);
        dec->SetVPOutputRef(1);
        if (eSCALE_RCS == scale_mode || eSCALE_VECS == scale_mode)  // SFC resize off
        {
            dec->SetDecPostProc(false);
            if (eSCALE_RCS == scale_mode)
                dec->SetFilterFlag(1);
            else
                dec->SetFilterFlag(0);
        }
        else if (eSCALE_VCS == scale_mode) // SFC resize on
        {
            dec->SetDecPostProc(true);
            dec->SetDecodeOutputRef(1);
            dec->SetVPOutFormat(MFX_FOURCC_NV12);
        }
        dec->SetVPRatio(vp_ratio);
        if (va_share)
            dec->SetVPOutFormat(MFX_FOURCC_NV12);
        dec->SetBatchSize(batch_num);
        dec->SetVPOutResolution(dshape_width, dshape_height);
        dec->SetVPDumpAllframe(dump_results);
        dec->SetCodecType(codec_type);
        dec->SetVPMemOutTypeVideo(va_share); // for one channel, use va surface sharing

        CHECK_STATUS(dec->Prepare());
        INFO("DecodeThreadBlock[%d]  prepared", i);

    }

    std::vector<std::unique_ptr<InferenceThreadBlock>> inferBlocks;
    std::vector<std::unique_ptr<VASinkPin>> emptySinks;
    std::vector<std::unique_ptr<VACsvWriterPin>> fileSinks;

    for (int i = 0; i < channel_num_infer; i++)
    {
        if (eSSD == detect_type)
            inferBlocks.push_back(std::make_unique<InferenceThreadBlock>(i, MOBILENET_SSD_U8));
        else if (eYOLO == detect_type)
            inferBlocks.push_back(std::make_unique<InferenceThreadBlock>(i, YOLO));

        auto& infer = inferBlocks[i];

        infer->ConnectInput(c1->NewOutputPin());
        if (perf_test)
        {
            emptySinks.push_back(std::make_unique<VASinkPin>());
            infer->ConnectOutput(emptySinks[i].get());
        }
        else
        {
            char filename[256] = {};
            if (batch_num > 1)
                sprintf(filename, "output.%02d.csv", i);
            else
                sprintf(filename, "output.csv");
            fileSinks.push_back(std::make_unique<VACsvWriterPin>(filename));
            infer->ConnectOutput(fileSinks[i].get());
        }

        infer->SetAsyncDepth(num_request);
        infer->SetStreamNum(num_stream);
        infer->SetBatchNum(batch_num);
        infer->SetModelInputReshapeWidth(dshape_width);
        infer->SetModelInputReshapeHeight(dshape_height);
        infer->SetConfidenceThreshold(dconf_threshold);
        infer->SetDevice("GPU");
        INFO("SetDevice  GPU");

        std::string model_file = model_name + ".xml";
        std::string coeff_file = model_name + ".bin";

        infer->SetModelFile(model_file.c_str(), coeff_file.c_str());
        INFO("SetModelFile    %s   %s ", model_file.c_str(), coeff_file.c_str());
        if (va_share)
        {
            infer->EnabelSharingWithVA();
            INFO("EnabelSharingWithVA ");
        }

        CHECK_STATUS(infer->Prepare());
    }

    VAThreadBlock::RunAllThreads();
    INFO("RunAllThreads");

    Statistics::getInstance().ReportPeriodly(1.0, duration);
    INFO("Statistics::getInstance().ReportPeriodly  duration %d ", duration);

    VAThreadBlock::StopAllThreads();
    INFO("StopAllThreads");

    TRACE("     del channel_num %d  ", channel_num)

    decodeBlocks.clear();
    filePins.clear();
    inferBlocks.clear();
    emptySinks.clear();
    fileSinks.clear();

    INFO("ObjectDetection test finished");

    return 0;
}
