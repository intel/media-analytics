GStreamer
=========

.. contents::

Overview
--------

GStreamer is a framework for creating streaming media applications.

The framework is based on plugins that will provide the various codec and other
functionality. The plugins can be linked and arranged in a pipeline. This
pipeline defines the flow of the data.

Please refer to the official `documentation <https://gstreamer.freedesktop.org/documentation/?gi-language=c>`_ for more details.

GStreamer packages that are present in the containers:

* gstreamer: the core package (gst-launch-1.0, filesrc, fakesink)
* gst-plugins-base: an essential exemplary set of elements (playbin, decodebin, videoconvert)
* gst-plugins-good: a set of good-quality plug-ins under LGPL (imagefreeze, qtdemux)
* gst-plugins-bad: a set of plug-ins that need more quality (h265parse, fpsdisplaysink)
* gstreamer-vaapi: a set of plug-ins that wrap libva for decoding and encoding (vaapih265dec, vaapih265enc)
* DL Streamer: a set of GStreamer plugins for visual analytics (gvadetect, gvaclassify)

Other packages not included into the container::

* gst-plugins-ugly: a set of good-quality plug-ins that might pose distribution problems

You can use next command to get help about a GStreamer plugin (its capabilities and parameters)::

  gst-inspect-1.0 <plugin>

Pipeline Examples
-----------------

#. Playback media file and let GStreamer detect input type::

     gst-launch-1.0 playbin uri=file://somefile.mp4

#. Source media file and let GStreamer automatically apply demuxer / decoder::

     gst-launch-1.0 filesrc location=somefile.mp4 ! decodebin ! fakesink

#. Source media file (MP4 container) with specified demuxer and decoder::

     gst-launch-1.0 filesrc location=somefile.mp4 ! qtdemux ! vaapih264dec ! \
      fakesink

#. Source video file (H265) and decode it::

     gst-launch-1.0 filesrc location=somefile.h265 ! h265parse ! vaapih265dec ! \
      fakesink

#. Source video file and specify output memory type and image format for decoder::

     gst-launch-1.0 filesrc location=somefile.h265 ! h265parse ! vaapih265dec ! \
      video/x-raw(memory:DMABuf),format=RGBA ! fakesink

#. Source video file, decode it and stop after 5th frame::

     gst-launch-1.0 filesrc location=somefile.h265 ! h265parse ! vaapih265dec ! \
      identity eos-after=5 ! fakesink

#. Source image file and play it 1000 times (decoding happens only onces)::

     gst-launch-1.0 filesrc location=somefile.jpeg ! jpegparse ! vaapijpegdec ! \
      imagefreeze num-buffers=1000 ! fakesink

#. Run object detection on source video file using GPU device and save results
   to json::

     gst-launch-1.0 filesrc location=somefile.h265 ! h265parse ! vaapih265dec ! \
      gvadetect model=somemodel.xml device=GPU ! gvametaconvert ! \
      gvametapublish file-path=results.json ! fakesink

#. Run object detection and classification, draw bounding boxes and labels and
   save result video to a file::

     gst-launch-1.0 filesrc location=somefile.h265 ! h265parse ! vaapih265dec ! \
      gvadetect model=detectionmodel.xml device=GPU ! \
      gvaclassify model=classificationmodel.xml device=GPU ! \
      gvawatermark ! encodebin ! filesink location=result.h265
