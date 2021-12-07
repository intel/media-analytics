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

#include <unistd.h>
#include <iostream>
#include <string>
#include "Inference.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>


#include "DataPacket.h"

const char *device = "GPU";

const char *image_file1[] = {
                            "../../clips/0.jpg",
                            "../../clips/1.jpg",
                            "../../clips/2.jpg",
                            "../../clips/3.jpg",
                            "../../clips/4.jpg"
                            };
const char *image_file2[] = {
                            "../../clips/5.jpg",
                            "../../clips/6.jpg",
                            "../../clips/7.jpg",
                            "../../clips/8.jpg",
                            "../../clips/9.jpg",
                            };

int InsertRGBFrame(InferenceBlock *infer, const cv::Mat &image, uint32_t channel, uint32_t frame, uint32_t inputId = 0)
{
    // The img in BGR format
    std::vector<cv::Mat> inputChannels;
    int channelNum = 3;
    int width = image.size[0];
    int height = image.size[1];

    uint8_t *input = new uint8_t[width * height * 3];
    for (int i = 0; i < channelNum; i ++)
    {
        cv::Mat channel(height, width, CV_8UC1, input + i * width * height);
        inputChannels.push_back(channel);
    }
    cv::split(image, inputChannels);

    infer->InsertImage(input, channel, frame, inputId);

    delete[] input;
    return 0;
}

int InsertRGBFrame2(InferenceBlock *infer, const cv::Mat &image, uint32_t channel, uint32_t frame, uint32_t inputId = 0)
{
    return infer->InsertImage(image, channel, frame, inputId);
}

int TestSSD()
{
    int ret = 0;
    auto deleter = [&](InferenceBlock* ptr){ InferenceBlock::Destroy(ptr); };
    std::unique_ptr<InferenceBlock, decltype(deleter)> infer(InferenceBlock::Create(MOBILENET_SSD_U8), deleter);
    infer->Initialize(5, 3);
    ret = infer->Load(device, "../../models/mobilenet-ssd.xml", "../../models/mobilenet-ssd.bin");
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    std::vector<cv::Mat> frames1;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread(image_file1[i], cv::IMREAD_COLOR);
        resize(src, frame, {300, 300});
        frames1.push_back(frame);
    }
    std::vector<cv::Mat> frames2;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread(image_file2[i], cv::IMREAD_COLOR);
        resize(src, frame, {300, 300});
        frames2.push_back(frame);
    }

    InsertRGBFrame(infer.get(), frames1[0], 0, 0);
    InsertRGBFrame(infer.get(), frames2[0], 1, 0);
    InsertRGBFrame(infer.get(), frames1[1], 0, 1);
    InsertRGBFrame(infer.get(), frames1[2], 0, 2);
    InsertRGBFrame(infer.get(), frames2[1], 1, 1);
    InsertRGBFrame(infer.get(), frames2[2], 1, 2);
    InsertRGBFrame(infer.get(), frames2[3], 1, 3);
    InsertRGBFrame(infer.get(), frames1[3], 0, 3);
    InsertRGBFrame(infer.get(), frames1[4], 0, 4);
    InsertRGBFrame(infer.get(), frames2[4], 1, 4);
    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer->GetOutput(outputs, channels, frames);
        //printf("%d outputs\n", outputs.size());
        if (ret == -1)
        {
            break;
        }
    }

    for (int i = 0; i < outputs.size(); i++)
    {
        int channel, frame;
        //printf("channel %d, frame %d\n", outputs[i]->ChannelIndex(), outputs[i]->FrameIndex());
    }
    
    cv::Mat screen = cv::Mat(300, 300, CV_8UC3);

    int index = 0;
    for (int i = 0; i < channels.size(); i++)
    {
        //printf("channel %d, frame %d\n", channels[i], frames[i]);

        if (channels[i] == 0)
        {
            frames1[frames[i]].copyTo(screen);
        }
        else if (channels[i] == 1)
        {
            frames2[frames[i]].copyTo(screen);
        }

        while (index < outputs.size()
            && outputs[index]->ChannelIndex() == channels[i]
            && outputs[index]->FrameIndex() == frames[i])
        {
            float left, top, right, bottom;
            outputs[index]->GetRoiRegion(&left, &top, &right, &bottom);
            uint32_t l, t, r, b;
            l = (uint32_t)(left * 300);
            t = (uint32_t)(top * 300);
            r = (uint32_t)(right * 300);
            b = (uint32_t)(bottom * 300);
            cv::rectangle(screen, cv::Point(l, t), cv::Point(r, b), cv::Scalar(71, 99, 250), 2);
            ++ index;
        }

        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        cv::imshow( "Display window", screen );
        cv::waitKey();
    }

    return 0;
}

int TestResnet()
{
    int ret = 0;
    auto deleter = [&](InferenceBlock* ptr){ InferenceBlock::Destroy(ptr); };
    std::unique_ptr<InferenceBlock, decltype(deleter)> infer(InferenceBlock::Create(RESNET_50), deleter);
    infer->Initialize(5, 3);
    ret = infer->Load(device, "../../models/resnet-50-128.xml", "../../models/resnet-50-128.bin");
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    std::vector<cv::Mat> frames1;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread("../../clips/cat.jpg", cv::IMREAD_COLOR);
        resize(src, frame, {224, 224});
        frames1.push_back(frame);
    }
    
    std::vector<cv::Mat> frames2;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread("../../clips/car.jpg", cv::IMREAD_COLOR);
        resize(src, frame, {224, 224});
        frames2.push_back(frame);
    }

    InsertRGBFrame(infer.get(), frames1[0], 0, 0);
    InsertRGBFrame(infer.get(), frames2[0], 1, 0);
    InsertRGBFrame(infer.get(), frames1[1], 0, 1);
    InsertRGBFrame(infer.get(), frames1[2], 0, 2);
    InsertRGBFrame(infer.get(), frames2[1], 1, 1);
    InsertRGBFrame(infer.get(), frames2[2], 1, 2);
    InsertRGBFrame(infer.get(), frames2[3], 1, 3);
    InsertRGBFrame(infer.get(), frames1[3], 0, 3);
    InsertRGBFrame(infer.get(), frames1[4], 0, 4);
    InsertRGBFrame(infer.get(), frames2[4], 1, 4);
    
    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer->GetOutput(outputs, channels, frames);
        //printf("%d outputs\n", outputs.size());
        if (ret == -1)
        {
            break;
        }
    }

    cv::Mat screen = cv::Mat(224, 224, CV_8UC3);

    int index = 0;
    for (int i = 0; i < channels.size(); i++)
    {
        //printf("channel %d, frame %d\n", channels[i], frames[i]);

        if (channels[i] == 0)
        {
            frames1[frames[i]].copyTo(screen);
        }
        else if (channels[i] == 1)
        {
            frames2[frames[i]].copyTo(screen);
        }

        printf("channel %d, frame %d, class %d, conf %f\n", outputs[i]->ChannelIndex(),
                                                            outputs[i]->FrameIndex(),
                                                            outputs[i]->Class(),
                                                            outputs[i]->Confidence());
        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        cv::imshow( "Display window", screen );
        cv::waitKey();
    }

    return 0;
}

int TestSISR()
{
    int ret = 0;
    const char *device_type = "CPU";
    const char* model_xml = (char *)"/home/fresh/data/model/single-image-super-resolution-1032.xml";
    const char* model_bin = (char *)"/home/fresh/data/model/single-image-super-resolution-1032.bin";
    const char* inputfile = (char *)"/home/fresh/data/model/test.jpg";

    auto deleter = [&](InferenceBlock* ptr){ InferenceBlock::Destroy(ptr); };
    std::unique_ptr<InferenceBlock, decltype(deleter)> infer(InferenceBlock::Create(SISR), deleter);
    infer->Initialize(1, 1);
    ret = infer->Load(device_type, model_xml , model_bin);
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    cv::Mat src = cv::imread(inputfile, cv::IMREAD_COLOR);
    InsertRGBFrame2(infer.get(), src, 0, 0, 0);

    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer->GetOutput(outputs, channels, frames);
        if (ret == -1)
            break;
    }

    return 0;
}

int TestRCAN()
{
    int ret = 0;
    const char *device_type = "CPU";
    const char* model_xml = (char*)"/home/fresh/data/model/rcan/rcan_360x640_scale2_resgroups5_resblocks8_feats64.xml";
    const char* model_bin = (char*)"/home/fresh/data/model/rcan/rcan_360x640_scale2_resgroups5_resblocks8_feats64.bin";
    const char* inputfile = (char*)"/home/fresh/data/model/rcan/test_rcan.jpg";

    auto deleter = [&](InferenceBlock* ptr){ InferenceBlock::Destroy(ptr); };
    std::unique_ptr<InferenceBlock, decltype(deleter)> infer(InferenceBlock::Create(RCAN), deleter);
    infer->Initialize(1, 1);
    ret = infer->Load(device_type, model_xml , model_bin);
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    cv::Mat src = cv::imread(inputfile, cv::IMREAD_COLOR);
    InsertRGBFrame2(infer.get(), src, 0, 0, 0);

    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer->GetOutput(outputs, channels, frames);
        if (ret == -1)
            break;
    }

    return 0;
}

using namespace cv;
using namespace std;
std::string input_filename;
std::string model_path;
std::string test_device;
static InferenceModelType modelType = MOBILENET_SSD_U8;

int Test()
{
    int ret = 0;
    uint32_t width;
    uint32_t height;
    char model_file[512], coeff_file[512];

    switch(modelType)
    {
        case MOBILENET_SSD_U8:
            width = 300;
            height = 300;
            sprintf(model_file, "%s/ssd_mobilenet_v2_coco_INT8.xml", model_path.c_str());
            sprintf(coeff_file, "%s/ssd_mobilenet_v2_coco_INT8.bin", model_path.c_str());
            break;
        case RESNET_50:
            width = 224;
            height = 224;
            sprintf(model_file, "%s/resnet_v1.5_50_i8.xml", model_path.c_str());
            sprintf(coeff_file, "%s/resnet_v1.5_50_i8.bin", model_path.c_str());
            break;
        case SISR:
            width = 480;
            height = 270;
            strcpy(model_file, "/home/fresh/data/model/single-image-super-resolution-1032.xml");
            strcpy(coeff_file, "/home/fresh/data/model/single-image-super-resolution-1032.bin");
            break;
        case RCAN:
            width = 640;
            height = 360;
            strcpy(model_file, "/home/fresh/data/model/rcan/rcan_360x640_scale2_resgroups5_resblocks8_feats64.xml");
            strcpy(coeff_file, "/home/fresh/data/model/rcan/rcan_360x640_scale2_resgroups5_resblocks8_feats64.bin");
            break;
        default:
            cout << "Model type error!\n";
            return -1;
    }

    auto deleter = [&](InferenceBlock* ptr){ InferenceBlock::Destroy(ptr); };
    std::unique_ptr<InferenceBlock, decltype(deleter)> infer(InferenceBlock::Create(modelType), deleter);
    infer->Initialize(1/*batch number*/, 1/*async depth*/);
    ret = infer->Load(test_device.c_str(), model_file, coeff_file);
    if (ret)
    {
        cout << "load model failed!\n";
        return ret;
    }

    VideoCapture cap(input_filename);
    if (!cap.isOpened())
    {
        cout << "Cannot open the video file. \n";
        return -1;
    }

    uint32_t frame_count = 0;
    while(1)
    {
        Mat src, frame;

        if (!cap.read(src))
            break;

        resize(src, frame, {(int)width, (int)height});

        InsertRGBFrame(infer.get(), frame, 0, frame_count);

        std::vector<VAData *> outputs;
        std::vector<uint32_t> channels;
        std::vector<uint32_t> frames;

        infer->GetOutput(outputs, channels, frames);
        for (int i = 0; i < outputs.size(); i++)
            printf("channel %d, frame %d, class %d, conf %f\n", outputs[i]->ChannelIndex(),
                outputs[i]->FrameIndex(),
                outputs[i]->Class(),
                outputs[i]->Confidence());

        frame_count++;
    }

    return ret;
}

void App_ShowUsage(void)
{
    cout << "Usage: InferenceOV -i input.mp4 [options]\n";
    cout << "           -ssd: running mobilenetSSD\n";
    cout << "           -resnet: running Resnet50\n";
    cout << "           -m model path \n";
    cout << "           -d device \n";
}

int ParseOpt(int argc, char *argv[])
{
    int sts = 0;

    if (argc < 2)
    {
        App_ShowUsage();
        return sts;
    }

    std::vector <std::string> sources;
    std::string arg = argv[1];

    if ((arg == "-h") || (arg == "--help"))
    {
        App_ShowUsage();
        return sts;
    }

    for (int i = 1; i < argc; ++i)
        sources.push_back(argv[i]);

    for (int i = 0; i < argc-1; ++i)
    {
        if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        else if (sources.at(i) == "-ssd")
            modelType = MOBILENET_SSD_U8;
        else if (sources.at(i) == "-resnet")
            modelType = RESNET_50;
        else if (sources.at(i) == "-m")
            model_path = sources.at(++i);
        else if (sources.at(i) == "-d")
            test_device = sources.at(++i);
    }

    if (input_filename.empty())
    {
        cout << "Missing input file name!!!!!!!\n";
        App_ShowUsage();
        return sts;
    }

    if (model_path.empty())
    {
        model_path = "../../models";
    }

    if (test_device.empty())
    {
        test_device = "GPU";
    }

    return sts;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        TestSSD();
        TestResnet();

        //TestSISR();
        //TestRCAN();
        return 0;
    }

    if (-1 == ParseOpt(argc, argv))
        return -1;


    if (-1 == Test())
        return -1;

    return 0;
}
