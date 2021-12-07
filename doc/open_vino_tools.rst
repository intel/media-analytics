OpenVINO tools
==============

.. contents::

Overview
--------

The OpenVINO Toolkit Open Model Zoo includes pre-trained optimized deep
learning models along with download and conversion tools.

The detailed listing of all publicly available models is `here
<https://github.com/openvinotoolkit/open_model_zoo/blob/master/models/public/index.md>`_.

A detailed overview of all Intel's pre-trained models is `here
<https://docs.openvinotoolkit.org/latest/omz_models_group_intel.html>`_.

Print all available models
--------------------------

Models available in the zoo can be listed with the following command::
  
  omz_downloader --print_all

Download available models
-------------------------

To download the tensorflow version of `ssd_mobilenet_v1_coco <https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/ssd_mobilenet_v1_coco>`_ model::

  omz_downloader --name ssd_mobilenet_v1_coco -o models

To download the tensorflow version of `resnet-50-tf <https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/resnet-50-tf>`_ model::

  omz_downloader --name resnet-50-tf -o models

Converting Available Models
---------------------------

Public models which are not stored in OpenVINO IR representation must be 
converted before use with the OpenVINO inference engine. 
For models that are already stored in OpenVINO IR (e.g. Intel models)
no conversion is required - they can be used right away.

To convert the ssd_mobilenet_v1_coco model into openvino IR use the command::

  omz_converter --name ssd_mobilenet_v1_coco -d models 

After execution of ``omz_converter`` command, the converted model 
in OV IR format will be created in directories ``FP32`` and ``FP16``::

  $ ls models/public/ssd_mobilenet_v1_coco/FP32/
  ssd_mobilenet_v1_coco.bin  ssd_mobilenet_v1_coco.mapping  ssd_mobilenet_v1_coco.xml

  $ ls models/public/ssd_mobilenet_v1_coco/FP16/
  ssd_mobilenet_v1_coco.bin  ssd_mobilenet_v1_coco.mapping  ssd_mobilenet_v1_coco.xml

To convert the resnet-50-tf model into opevino IR use the command::

  omz_converter --name resnet-50-tf -d models 

After execution of the above command, the converted model 
in OV IR format will be created in directories ``FP32`` and ``FP16``::

  $ ls models/public/resnet-50-tf/FP32/
  resnet-50-tf.bin  resnet-50-tf.mapping  resnet-50-tf.xml

  $ ls models/public/resnet-50-tf/FP16/
  resnet-50-tf.bin  resnet-50-tf.mapping  resnet-50-tf.xml

The above models (FP32/ FP16) can now be used to execute inference applications using open vino backend.

Using open vino IR models in inference applications
---------------------------------------------------

Object Detection 
^^^^^^^^^^^^^^^^
Using *VA Sample* ::

  ObjectDetection -m_detect models/public/ssd_mobilenet_v1_coco/FP32/ssd_mobilenet_v1_coco \
    -codec 264 -c 1 \
    -i /opt/data/embedded/pexels-1388365.h264 \ 
    -infer 1 -b 1 -r 1 \
    -scale fast \
    -d -t 3 

Using *gstreamer/DLStreamer* ::

  gst-launch-1.0 filesrc location=/opt/data/embedded/pexels-1388365.h264 ! \
    video/x-h264 ! h264parse ! video/x-h264 ! vaapih264dec ! video/x-raw\(memory:VASurface\) ! \
    gvadetect device=GPU nireq=1 batch-size=1 pre-process-backend=vaapi inference-interval=1 \
      model=models/public/ssd_mobilenet_v1_coco/FP32/ssd_mobilenet_v1_coco.xml \
      model-instance-id=detect ! \
    gvametaconvert ! gvametapublish file-path=results.jsonl method=file file-format=json-lines ! \
    gvafpscounter ! fakesink async=false

Sample Pipeline
^^^^^^^^^^^^^^^
Using *VA Sample* ::

  SamplePipeline \
    -m_detect models/public/ssd_mobilenet_v1_coco/FP32/ssd_mobilenet_v1_coco \
    -m_classify models/public/resnet-50-tf/FP32/resnet-50-tf  \
    -codec 264 -c 1 \
    -i /opt/data/embedded/pexels-1388365.h264
    -infer 1  -b 1 -r 1 \
    -scale fast \
    -d -t 3

Using *gstreamer/DLStreamer* ::
    
  gst-launch-1.0 filesrc location=/opt/data/embedded/pexels-1388365.h264 ! \
    video/x-h264 ! h264parse ! video/x-h264 ! vaapih264dec name=decode0 ! video/x-raw\(memory:VASurface\) ! \
    gvaclassify device=GPU nireq=1 batch-size=1 pre-process-backend=vaapi inference-interval=1 \
      model=models/public/resnet-50-tf/FP32/resnet-50-tf.xml model-instance-id=classify \
      inference-region=full-frame ! \
    gvametaconvert ! gvametapublish file-path=results.jsonl method=file file-format=json-lines ! \
    gvafpscounter ! fakesink async=false

