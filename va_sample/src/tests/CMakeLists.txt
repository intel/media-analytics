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

set(MFX_INCLUDE ${MFX_INCLUDE_DIRS})
include_directories( ${MFX_INCLUDE})

set(VA_SOURCES "")
set(VA_INCLUDES "")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(${CMAKE_CURRENT_LIST_DIR}/../execution/va_srcs.cmake)

link_directories(/opt/intel/mediasdk/lib/)
link_directories(/opt/intel/mediasdk/lib64/)

include(${CMAKE_CURRENT_LIST_DIR}/../function/va_srcs.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../common/va_srcs.cmake)

add_executable(DecodeCropTest DecodeCrop_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}")
target_link_libraries(DecodeCropTest pthread mfx va va-drm)
install(TARGETS DecodeCropTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include_directories(${CMAKE_CURRENT_LIST_DIR}/../../libs/inference)

add_executable(InferenceOV InferenceOV_test.cpp)
target_link_libraries( InferenceOV detect opencv_highgui)
install(TARGETS InferenceOV RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(InferenceBlockTest ${CMAKE_CURRENT_LIST_DIR}/InferenceThreadBlock_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}" )
target_link_libraries(InferenceBlockTest pthread mfx va va-drm detect)
install(TARGETS InferenceBlockTest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ObjectClassification ${CMAKE_CURRENT_LIST_DIR}/ObjectClassification.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}" "${DISPLAY_SOURCES}" )
target_link_libraries(ObjectClassification pthread mfx va va-drm detect opencv_highgui)
install(TARGETS ObjectClassification RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ObjectDetection ${CMAKE_CURRENT_LIST_DIR}/ObjectDetection.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}" "${DISPLAY_SOURCES}" )
target_link_libraries(ObjectDetection pthread mfx va va-drm detect opencv_highgui)
install(TARGETS ObjectDetection RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(SamplePipeline ${CMAKE_CURRENT_LIST_DIR}/SamplePipeline.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}")
target_link_libraries(SamplePipeline pthread mfx va va-drm detect)
install(TARGETS SamplePipeline RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(TranscodeSR ${CMAKE_CURRENT_LIST_DIR}/Decode_SR_Encode.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${ENCODE_SOURCES}" "${INFER_SOURCES}" )
target_link_libraries(TranscodeSR pthread mfx va va-drm detect igfxcmrt)
install(TARGETS TranscodeSR RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (DEFINED MSDK2_0)
    add_executable(DecodeCropTestVpl DecodeCrop_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES2}")
    target_compile_definitions(DecodeCropTestVpl PRIVATE MSDK_2_0_API)
    target_link_libraries(DecodeCropTestVpl pthread mfx va va-drm)
    install(TARGETS DecodeCropTestVpl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

