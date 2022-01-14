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

#ifndef MSDK_2_0_API
#include "DecodeThreadBlock.h"
#else
#include "DecodeThreadBlock2.h"
#endif
#include "CropThreadBlock.h"
#include "Statistics.h"
#include "ConnectorRR.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>

enum eSCALE_mode
{
    eSCALE_RCS = 0,
    eSCALE_VCS,
    eSCALE_VECS,
    eSCALE_Max
};

enum eFRAME_filter
{
    eFILTER_all,
    eFILTER_odd,
    eFILTER_Max
};

enum eCROP_mode
{
    eCROP_DEFAULT = 0,
    eCROP_HQ,
    eCROP_Max
};

static int vp_ratio = 1;
static int channel_num = 1;
static eSCALE_mode scale_mode = eSCALE_VECS;
static eCROP_mode crop_mode = eCROP_DEFAULT;
static int vpp_width = 300;
static int vpp_height = 300;
static int crop_width = 224;
static int crop_height = 224;
static int crop_x = 0;
static int crop_y = 0;
static int crop_w = 0;
static int crop_h = 0;
std::string input_filename;
std::string output_filename;
static int vpp_output_fourcc = MFX_FOURCC_RGBP;
static mfxU32 codec_type = MFX_CODEC_AVC;
static int duration = -1;
static int dumpDecOrVp = 1; //0 not dump, 1 VP, 2 dec
static bool dump_vp = true;
static uint32_t frame_number = 0;
static eFRAME_filter frame_filter = eFILTER_all;

void App_ShowUsage(void)
{
    printf("Usage: DecodeCrop_test -i input.264 [options]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help             Print this help\n");
    printf("  -codec 264|265         Use this codec to decode the video (default: 264)\n");
    printf("  -c dec_channels        Number of decoding channels (default: 1)\n");
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
    printf("  -scale_frames all|odd  Apply scaling for the specified frames (default: all)\n");
    printf("  -t seconds             How many seconds this app should run\n");
    printf("  -p                     Performance mode, no files dumped\n");
    printf("  -vp_res w h            VP output resolution (default: %d %d)\n", vpp_width, vpp_height);
    printf("  -crop x y w h          ROI region in the cropping");
    printf("  -crop_res w h          Cropping output resolution (default: %d %d)\n", crop_width, crop_height);
    printf("  -f nv12|rgbp           Output format for VP and cropping (default: %s)\n",
      (vpp_output_fourcc == MFX_FOURCC_RGBP)? "rgbp": "nv12");
    printf("  -o:dec filename        Decode output file name\n");
    printf("  -o:scale filename      Scale output file name\n");
    printf("  -n frame_num           Specify how many frames to be decoded\n");
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
        if (sources.at(i) == "-c")
            channel_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        else if (sources.at(i) == "-r")
            vp_ratio = stoi(sources.at(++i));
        else if (sources.at(i) == "-vp_res")
        {
            vpp_width = stoi(sources.at(++i));
            vpp_height = stoi(sources.at(++i));
        }
        else if (sources.at(i) == "-crop_res")
        {
            crop_width = stoi(sources.at(++i));
            crop_height = stoi(sources.at(++i));
        }
        else if (sources.at(i) == "-crop")
        {
            crop_x = stoi(sources.at(++i));
            crop_y = stoi(sources.at(++i));
            crop_w = stoi(sources.at(++i));
            crop_h = stoi(sources.at(++i));
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
        else if(sources.at(i) == "-scale_frames")
        {
            std::string scale_frames = sources.at(++i);
            if(scale_frames == "odd")
                frame_filter = eFILTER_odd;
        }
        else if (sources.at(i) == "-f")
        {
            std::string format = sources.at(++i);
            if (format == "nv12")
                vpp_output_fourcc = MFX_FOURCC_NV12;
            else if (format == "rgbp")
                vpp_output_fourcc = MFX_FOURCC_RGBP;
        }
        else if (sources.at(i) == "-t")
            duration = stoi(sources.at(++i));
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
        else if (sources.at(i) == "-p")
        {
            dump_vp = false;
            dumpDecOrVp = 0;
        }
        else if(sources.at(i) == "-o")
        {
            output_filename = sources.at(++i);
        }
        else if(sources.at(i) == "-o:dec")
        {
            output_filename = sources.at(++i);
            dumpDecOrVp = 2;
        }
        else if(sources.at(i) == "-o:scale")
        {
            output_filename = sources.at(++i);
            dumpDecOrVp = 1;
        }
        else if(sources.at(i) == "-n")
        {
            frame_number = stoi(sources.at(++i));
        }
        else
        {
            printf("unknown argument: %s\n", sources.at(i).c_str());
            App_ShowUsage();
            exit(0);
        }
    }

    if (input_filename.empty())
    {
        printf("Missing input file name!!!!!!!\n");
        App_ShowUsage();
        exit(0);
    }

    if(channel_num > 1 && !output_filename.empty())
    {
        printf("DecodeCrop_test can't support to dump multi-channel outputs, exiting!\n");
        exit(0);
    }
}

class DummyObjectDetection : public VAThreadBlock
{
public:
    DummyObjectDetection():
        m_inputWidth(0),
        m_inputHeight(0),
        m_cropx(0),
        m_cropy(0),
        m_cropw(0),
        m_croph(0)
    {
    }

    ~DummyObjectDetection()
    {
    }

    inline void SetInputResolution(uint32_t width, uint32_t height)
    {
        m_inputWidth = width;
        m_inputHeight = height;
    }
    inline void SetROIRegion(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
    {
        m_cropx = x;
        m_cropy = y;
        m_cropw = w;
        m_croph = h;
    }

    int PrepareInternal()
    {
        m_l = (float)m_cropx/(float)m_inputWidth;
        m_t = (float)m_cropy/(float)m_inputHeight;
        m_r = (float)(m_cropx + m_cropw)/(float)m_inputWidth;
        m_b = (float)(m_cropy + m_croph)/(float)m_inputHeight;
        if (m_r > 1.0)
            m_r = 1.0;
        if (m_b > 1.0)
            m_b = 1.0;
        return 0;
    }

    int Loop()
    {
        while (true)
        {
            VADataPacket *InPacket = AcquireInput();
            VADataPacket *OutPacket = DequeueOutput();

            if (!InPacket || !OutPacket)
                break;

            if (!InPacket->size())
                continue;

            uint32_t channel = 0, frame = 0;
            for (auto ite = InPacket->begin(); ite != InPacket->end(); ite ++)
            {
                VAData *data = *ite;
                channel = data->ChannelIndex();
                frame = data->FrameIndex();
                OutPacket->push_back(data);
            }

            ReleaseInput(InPacket);

            VAData *out = VAData::Create(m_l, m_t, m_r, m_b, 0, 1.0);
            out->SetID(channel, frame);
            out->SetRef(1);
            OutPacket->push_back(out);
            EnqueueOutput(OutPacket);
        }

        return 0;
    }

protected:
    uint32_t m_inputWidth;
    uint32_t m_inputHeight;
    uint32_t m_cropx;
    uint32_t m_cropy;
    uint32_t m_cropw;
    uint32_t m_croph;

    float m_l;
    float m_t;
    float m_r;
    float m_b;

};


int main(int argc, char *argv[])
{
    ParseOpt(argc, argv);

    std::vector<std::unique_ptr<DecodeThreadBlock>> decodeBlocks;
    std::vector<std::unique_ptr<CropThreadBlock>> cropBlocks;
    std::vector<std::unique_ptr<VAFilePin>> filePins;
    std::vector<std::unique_ptr<VASinkPin>> sinks;

    DummyObjectDetection *od = new DummyObjectDetection();
    VAConnectorRR *c1 = new VAConnectorRR(channel_num, 1, 10);
    VAConnectorRR *c2 = new VAConnectorRR(1, channel_num, 10);

    uint32_t decodeWidth = 0;
    uint32_t decodeHeight = 0;
    for (int i = 0; i < channel_num; i++)
    {
        decodeBlocks.push_back(std::make_unique<DecodeThreadBlock>(i));
        cropBlocks.push_back(std::make_unique<CropThreadBlock>(i));
        filePins.push_back(std::make_unique<VAFilePin>(input_filename.c_str(), !dump_vp));
        sinks.push_back(std::make_unique<VASinkPin>());

        auto& d = decodeBlocks[i];
        auto& c = cropBlocks[i];
        auto& pin = filePins[i];
        auto& sink = sinks[i];

        d->ConnectInput(pin.get());
        d->ConnectOutput(c1->NewInputPin());

        c->ConnectInput(c2->NewOutputPin());
        c->ConnectOutput(sink.get());

        // init decoder
        d->SetDecodeOutputRef(1);
        d->SetVPOutputRef(1);
        if (eSCALE_RCS == scale_mode || eSCALE_VECS == scale_mode)  // SFC resize off
        {
            d->SetDecPostProc(false);
            if(eSCALE_RCS == scale_mode)
                d->SetFilterFlag(1);
            else
                d->SetFilterFlag(0);
        }
        else if (eSCALE_VCS == scale_mode) // SFC resize on
        {
            if(frame_filter == eFILTER_all)
                d->SetDecPostProc(true);
            else
            {
                if (1 == (i%2))
                    d->SetDecPostProc(true);
            }
        }
        d->SetVPRatio(vp_ratio);
        d->SetVPOutFormat(vpp_output_fourcc);
        d->SetVPOutResolution(vpp_width, vpp_height);
        d->SetVPDumpAllframe(dumpDecOrVp);

        d->SetDumpFileName(output_filename);
        if (!dump_vp)
            d->SetVPOutputRef(0);
        d->SetCodecType(codec_type);
        d->SetFrameNumber(frame_number);
        CHECK_STATUS(d->Prepare());
        d->GetDecodeResolution(&decodeWidth, &decodeHeight);
        if (decodeWidth == 0 || decodeHeight == 0)
        {
            ERRLOG("No invalid decoding resolution!\n");
            goto clean;
        }

        // init cropping
        if (crop_w > 0 && crop_h > 0)
        {
            c->SetInputResolution(decodeWidth, decodeHeight);
            c->SetOutResolution(crop_width, crop_height);
            c->SetOutFormat(vpp_output_fourcc);
            c->SetOutDump(dump_vp);
            c->SetPipeFlag(crop_mode);
            CHECK_STATUS(c->Prepare());
            c->SetKeepAspectRatioFlag(false);
        }
        else
        {
            d->ConnectOutput(sink.get());
        }
    }

    // init the od 
    if (crop_w > 0 && crop_h > 0)
    {
        od->ConnectInput(c1->NewOutputPin());
        od->ConnectOutput(c2->NewInputPin());
        od->SetInputResolution(decodeWidth, decodeHeight);
        od->SetROIRegion(crop_x, crop_y, crop_w, crop_h);
        CHECK_STATUS(od->Prepare());
    }

    Statistics::getInstance().CountDownStart(channel_num);

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0, duration);

    VAThreadBlock::StopAllThreads();

clean:
    decodeBlocks.clear();
    filePins.clear();
    cropBlocks.clear();
    sinks.clear();

    delete c1;
    delete c2;
    delete od;

    return 0;
}
