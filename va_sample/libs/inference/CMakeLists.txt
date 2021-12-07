#
# Copyright (c) 2019, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

set(OpenCV_LIBS
  opencv_core
  opencv_video
  opencv_videoio
  opencv_imgproc
  opencv_photo
  opencv_highgui
  opencv_imgcodecs)

include_directories( ${CMAKE_CURRENT_LIST_DIR}/../../src/execution ${MFX_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/../../src/common)

add_library(detect SHARED
  Inference.h
  Inference.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/common/logs.cpp
  InferenceOV.cpp
  InferenceMobileSSD.cpp
  InferenceResnet50.cpp
  InferenceSISR.cpp
  InferenceRCAN.cpp
  InferenceYOLO.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/execution/DataPacket.cpp)

target_link_libraries(detect
  #${OpenCV_LIBS}
  opencv_core
  opencv_imgproc
  openvino::runtime
  OpenCL
  Threads::Threads)

install(TARGETS detect LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
