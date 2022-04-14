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
#include <memory>
#include <string>

#include "DataPacket.h"
#include "ConnectorRR.h"
#include "CropThreadBlock.h"
#include "DecodeThreadBlock.h"
#include "InferenceThreadBlock.h"
#include "Statistics.h"
#include "logs.h"

enum eSCALE_mode
{
    eSCALE_RCS = 0,
    eSCALE_VCS,
    eSCALE_VECS,
    eSCALE_Max
};

enum eCROP_mode
{
    eCROP_DEFAULT = 0,
    eCROP_HQ,
    eCROP_Max
};

enum eDETECT_type
{
    eSSD = 0,
    eYOLO
};

static int vp_ratio = 1;
static int channel_num = 1;
static int batch_num =1;
std::string input_filename;
std::string model_classify;
std::string model_detect;
static eDETECT_type detect_type = eSSD;
static int dump_crop = false;
static int inference_num = 1;
static int crop_num = 1;
static int classification_num = 1;
static int duration = -1;
static bool perf_test = false;
static bool va_share = false;
static bool va_sync = false;
static int num_request = 1;
static int num_stream = 0;
static float dconf_threshold = 0.8;
static mfxU32 codec_type = MFX_CODEC_AVC;
static eSCALE_mode scale_mode = eSCALE_VECS;
static eCROP_mode crop_mode = eCROP_DEFAULT;

static const char* default_detect_model = "/opt/intel/samples/models/ssd_mobilenet_v1_coco_INT8/ssd_mobilenet_v1_coco";
static const char* default_classify_model = "/opt/intel/samples/models/resnet-50-tf_INT8/resnet-50-tf_i8";
static const char* default_detect_type = "ssd";
static const float default_dconf_threshold = 0.8;
static const uint32_t default_ssd_input_width = 300;
static const uint32_t default_ssd_input_height = 300;
static const uint32_t default_yolo_input_reshape_width = 416;
static const uint32_t default_yolo_input_reshape_height = 416;
static const uint32_t default_resnet_input_width = 224;
static const uint32_t default_resnet_input_height = 224;
static uint32_t dshape_width = 0;
static uint32_t dshape_height = 0;

void App_ShowUsage(void)
{
    printf("Usage: SamplePipeline -i input.264 [<options>]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help             Print this help\n");
    printf("  -codec 264|265         Use this codec to decode the video (default: 264)\n");
    printf("  -c dec_channels        Number of decoding channels (default: 1)\n");
    printf("  -m_detect model        xml model file name with absolute path, no .xml needed\n");
    printf("                           default: %s\n", default_detect_model);
    printf("  -detect_type ssd|yolo  Detection type\n");
    printf("                           default: %s\n", default_detect_type);
    printf("  -dshape_w width        Detection model input reshape width\n");
    printf("                           ssd default: %d\n", default_ssd_input_width);
    printf("                           yolo default: %d\n", default_yolo_input_reshape_width);
    printf("  -dshape_h height       Detection model input reshape height\n");
    printf("                           ssd default: %d\n", default_ssd_input_height);
    printf("                           yolo default: %d\n", default_yolo_input_reshape_height);
    printf("  -dconf threshold       Minimum detection output confidence, range [0-1] (default: %.1f)\n", default_dconf_threshold);
    printf("  -m_classify model      xml model file name with absolute path, no .xml needed\n");
    printf("                           default: %s\n", default_classify_model);
    printf("  -b batch_number        Batch number in the inference model (default: 1)\n");
    printf("  -nireq req_number      Set the inference request number (default: 1)\n");
    printf("  -nstreams stream_num   Set the inference stream number (default: 0)\n");
    printf("  -r vp_ratio            Ratio of decoded frames to vp frames (default: 1)\n");
    printf("                           -r 0   disables vp\n");
    printf("                           -r 2   means doing vp every other frame\n");
    printf("  -scale hq|fast_inplace|fast\n");
    printf("    hq           - run scaling on EUs thru rcs or ccs depending on the platform\n");
    printf("    fast_inplace - run scaling with affinity to decoding, i.e. on the same vcs; scaling is done thru SFC\n");
    printf("    fast         - run scaling via vecs, scaling is done thru SFC (this is default)\n");
    printf("  -crop_mode hq|fast\n");
    printf("    hq           - run cropping on EUs thru rcs or ccs depending on the platform\n");
    printf("    fast         - run cropping via vecs, scaling is done thru SFC (this is default)\n");
    printf("  -t seconds             How many seconds this app should run\n");
    printf("  -d                     Dump the inference input, raw data of decode and vp\n");
    printf("  -p                     Performance mode, don't dump the csv results\n");
    printf("  -va_share              Share surface between media and inferece\n");
    printf("  -show                  Display every frame\n");
    printf("  -ssd                   ssd model thread number\n");
    printf("  -crop                  Crop thread number\n");
    printf("  -resnet                resnet thread number\n");
    printf("  -va_sync               Force vaSyncSurface() call in cropping thread block\n");
}

void ParseOpt(int argc, char *argv[])
{
    TRACE("input argc %d", argc);
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
        if (sources.at(i) == "-c")
            channel_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-b")
            batch_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        else if (sources.at(i) == "-r")
            vp_ratio = stoi(sources.at(++i));
        else if (sources.at(i) == "-d")
            dump_crop = true;
        else if (sources.at(i) == "-p")
            perf_test = true;
        else if (sources.at(i) == "-va_share")
            va_share = true;
        else if (sources.at(i) == "-va_sync")
            va_sync = true;
        else if (sources.at(i) == "-ssd")
            inference_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-crop")
            crop_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-resnet")
            classification_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-t")
            duration = stoi(sources.at(++i));
        else if (sources.at(i) == "-m_classify")
            model_classify = sources.at(++i);
        else if (sources.at(i) == "-m_detect")
            model_detect = sources.at(++i);
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
        else if (sources.at(i) == "-scale")
        {
            std::string engine = sources.at(++i);
            if(engine == "hq")
            {
                scale_mode = eSCALE_RCS;
            }
            else if(engine == "fast_inplace")
            {
                scale_mode = eSCALE_VCS;
            }
            else if(engine == "fast")
            {
                scale_mode = eSCALE_VECS;
            }
        }
        else if (sources.at(i) == "-crop_mode")
        {
            std::string engine = sources.at(++i);
            if(engine == "hq")
            {
                crop_mode = eCROP_HQ;
            }
            else if(engine == "fast")
            {
                crop_mode = eCROP_DEFAULT;
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
        dump_crop = false;

    if (input_filename.empty())
    {
        printf("Missing input file name!!!!!!!\n");
        App_ShowUsage();
        exit(0);
    }
    if (model_detect.empty())
    {
        model_detect = default_detect_model;
    }
    if (model_classify.empty())
    {
        model_classify = default_classify_model;
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
    std::vector<std::unique_ptr<InferenceThreadBlock>> inferBlocks;
    std::vector<std::unique_ptr<CropThreadBlock>> cropBlocks;
    std::vector<std::unique_ptr<InferenceThreadBlock>> classBlocks;
    std::vector<std::unique_ptr<VAFilePin>> filePins;
    std::vector<std::unique_ptr<VACsvWriterPin>> fileSinks;
    std::vector<std::unique_ptr<VASinkPin>> emptySinks;

    std::unique_ptr<VAConnectorRR> c1 = std::make_unique<VAConnectorRR>(channel_num, inference_num, 10);
    std::unique_ptr<VAConnectorRR> c2 = std::make_unique<VAConnectorRR>(inference_num, crop_num, 10);
    std::unique_ptr<VAConnectorRR> c3 = std::make_unique<VAConnectorRR>(crop_num, classification_num, 10);

    uint32_t decodeWidth = 0;
    uint32_t decodeHeight = 0;

    for (int i = 0; i < channel_num; i++)
    {
        INFO("intput file is %s\n", input_filename.c_str());

        decodeBlocks.push_back(std::make_unique<DecodeThreadBlock>(i));
        filePins.push_back(std::make_unique<VAFilePin>(input_filename.c_str(), perf_test));

        auto& dec = decodeBlocks[i];
        auto& pin = filePins[i];

        dec->ConnectInput(pin.get());
        dec->ConnectOutput(c1->NewInputPin());
        dec->SetDecodeOutputRef(0);
        dec->SetCodecType(codec_type);
        dec->SetVPOutputRef(1);
        if (eSCALE_RCS == scale_mode || eSCALE_VECS == scale_mode)  // SFC resize off
        {
            dec->SetDecPostProc(false);
            if(eSCALE_RCS == scale_mode)
                dec->SetFilterFlag(1);
            else
                dec->SetFilterFlag(0);
        }
        else if (eSCALE_VCS == scale_mode) // SFC resize on
        {
            dec->SetDecPostProc(true);
        }
        dec->SetDecodeOutputWithVP(); // when there is vp output, decode output also attached
        dec->SetVPRatio(vp_ratio);
        dec->SetVPOutResolution(dshape_width, dshape_height);
        if (va_share)
        {
            dec->SetVPOutFormat(MFX_FOURCC_NV12);
            dec->SetVPMemOutTypeVideo(true);
        }
        dec->SetBatchSize(batch_num * 2); // two inference after
        CHECK_STATUS(dec->Prepare());
        dec->GetDecodeResolution(&decodeWidth, &decodeHeight);
        if (decodeWidth == 0 || decodeHeight == 0)
        {
            ERRLOG("No invalid decoding resolution!\n");
            goto clean;
        }

        INFO("DecodeThreadBlock[%d}  prepared ", i);
    }
    INFO("After decoder prepare\n");

    for (int i = 0; i < inference_num; i++)
    {
        if (eSSD == detect_type)
            inferBlocks.push_back(std::make_unique<InferenceThreadBlock>(i, MOBILENET_SSD_U8));
        else if (eYOLO == detect_type)
            inferBlocks.push_back(std::make_unique<InferenceThreadBlock>(i, YOLO));

        auto& infer = inferBlocks[i];

        infer->ConnectInput(c1->NewOutputPin());
        infer->ConnectOutput(c2->NewInputPin());
        infer->SetAsyncDepth(num_request);
        infer->SetStreamNum(num_stream);
        infer->SetBatchNum(batch_num);
        infer->SetModelInputReshapeWidth(dshape_width);
        infer->SetModelInputReshapeHeight(dshape_height);
        infer->SetConfidenceThreshold(dconf_threshold);
        infer->SetOutputRef(2);
        infer->SetDevice("GPU");

        std::string model_file = model_detect+".xml";
        std::string coeff_file = model_detect+".bin";

        infer->SetModelFile(model_file.c_str(), coeff_file.c_str());
        if (va_share)
        {
            infer->EnabelSharingWithVA();
        }
        CHECK_STATUS(infer->Prepare());
    }
    INFO("After inference  prepared");

    for (int i = 0; i < crop_num; i++)
    {
        cropBlocks.push_back(std::make_unique<CropThreadBlock>(i));
        auto& crop = cropBlocks[i];

        crop->ConnectInput(c2->NewOutputPin());
        crop->ConnectOutput(c3->NewInputPin());
        crop->SetInputResolution(decodeWidth, decodeHeight);
        crop->SetOutResolution(default_resnet_input_width, default_resnet_input_height);
        crop->SetBatchSize(batch_num);
        if (va_share)
        {
            crop->SetOutFormat(MFX_FOURCC_NV12);
            crop->SetVPMemOutTypeVideo(true);
        }
        crop->SetVASync(va_sync);
        crop->SetOutDump(dump_crop);
        crop->SetPipeFlag(crop_mode);
        CHECK_STATUS(crop->Prepare());
    }
    INFO("After crop  prepared ");

    for (int i = 0; i < classification_num; i++)
    {
        classBlocks.push_back(std::make_unique<InferenceThreadBlock>(i, RESNET_50));
        auto& cla = classBlocks[i];

        cla->ConnectInput(c3->NewOutputPin());
        if (perf_test)
        {
            emptySinks.push_back(std::make_unique<VASinkPin>());
            cla->ConnectOutput(emptySinks[i].get());
        }
        else
        {
            char filename[256] = {};
            if (batch_num > 1)
                sprintf(filename, "output.%02d.csv", i);
            else
                sprintf(filename, "output.csv");
            fileSinks.push_back(std::make_unique<VACsvWriterPin>(filename));
            cla->ConnectOutput(fileSinks[i].get());
        }        
        cla->SetAsyncDepth(num_request);
        cla->SetStreamNum(num_stream);
        cla->SetBatchNum(batch_num);
        cla->SetDevice("GPU");

        std::string model_file = model_classify+".xml";
        std::string coeff_file = model_classify+".bin";

        cla->SetModelFile(model_file.c_str(), coeff_file.c_str());

        if (va_share)
        {
            cla->EnabelSharingWithVA();
        }
        CHECK_STATUS(cla->Prepare());
    }
    INFO("After classification  prepared ");

    VAThreadBlock::RunAllThreads();
    INFO("RunAllThreads");
    Statistics::getInstance().ReportPeriodly(1.0, duration);

    VAThreadBlock::StopAllThreads();

    INFO("StopAllThreads ");

clean:
    decodeBlocks.clear();
    filePins.clear();
    inferBlocks.clear();
    cropBlocks.clear();
    classBlocks.clear();
    emptySinks.clear();
    fileSinks.clear();

    INFO("SamplePipeline test finished \n");
    return 0;
}

