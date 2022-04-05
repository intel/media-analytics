dnl BSD 3-Clause License
dnl
dnl Copyright (c) 2020, Intel Corporation
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are met:
dnl
dnl * Redistributions of source code must retain the above copyright notice, this
dnl   list of conditions and the following disclaimer.
dnl
dnl * Redistributions in binary form must reproduce the above copyright notice,
dnl   this list of conditions and the following disclaimer in the documentation
dnl   and/or other materials provided with the distribution.
dnl
dnl * Neither the name of the copyright holder nor the names of its
dnl   contributors may be used to endorse or promote products derived from
dnl   this software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
dnl AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
dnl SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
dnl CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
dnl OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
include(begin.m4)

DECLARE(`OPENCV_VER',4.5.5)
DECLARE(`OPENCV_EIGEN',true)
DECLARE(`OPENCV_GTK2',false)
DECLARE(`OPENCV_OPENEXR',false)
DECLARE(`OPENCV_OPENJPEG',false)
DECLARE(`OPENCV_GSTREAMER',true)
DECLARE(`OPENCV_JASPER',false)

define(`OPENCV_EIGEN_BUILD',dnl
ifelse(OPENCV_EIGEN,true,`ifelse(
OS_NAME,ubuntu,libeigen3-dev,
OS_NAME,centos,eigen3-devel)'))dnl

define(`OPENCV_EIGEN_INSTALL',dnl
ifelse(OPENCV_EIGEN,true,`ifelse(
OS_NAME,ubuntu,libeigen3-dev,
OS_NAME,centos,gtk2-devel)'))dnl

define(`OPENCV_GTK2_BUILD',dnl
ifelse(OPENCV_GTK2,true,`ifelse(
OS_NAME,ubuntu,libgtk2.0-dev,
OS_NAME,centos,gtk2-devel)'))dnl

define(`OPENCV_GTK2_INSTALL',dnl
ifelse(OPENCV_GTK2,true,`ifelse(
OS_NAME,ubuntu,libgtk2.0-0,
OS_NAME,centos,gtk2)'))dnl

define(`OPENCV_OPENJPEG_BUILD',dnl
ifelse(OPENCV_OPENJPEG,true,`ifelse(
OS_NAME,ubuntu,libopenjp2-7-dev,
OS_NAME,centos,openjpeg2-devel)'))dnl

define(`OPENCV_OPENJPEG_INSTALL',dnl
ifelse(OPENCV_OPENJPEG,true,`ifelse(
OS_NAME,ubuntu,libopenjp2-7,
OS_NAME,centos,openjpeg2)'))dnl

ifelse(OS_NAME,ubuntu,dnl
`define(`OPENCV_BUILD_DEPS',`ca-certificates gcc g++ make wget cmake pkg-config OPENCV_EIGEN_BUILD OPENCV_GTK2_BUILD OPENCV_OPENJPEG_BUILD')'
`define(`OPENCV_INSTALL_DEPS',`OPENCV_EIGEN_INSTALL OPENCV_GTK2_INSTALL OPENCV_OPENJPEG_INSTALL')'
)

ifelse(OS_NAME,centos,dnl
`define(`OPENCV_BUILD_DEPS',`gcc gcc-c++ make wget cmake OPENCV_EIGEN_BUILD OPENCV_GTK2_BUILD OPENCV_OPENJPEG_BUILD')'
`define(`OPENCV_INSTALL_DEPS',`OPENCV_EIGEN_INSTALL OPENCV_GTK2_INSTALL OPENCV_OPENJPEG_INSTALL')'
)

ifelse(OPENCV_GSTREAMER,true,`include(gst-plugins-base.m4)')dnl

define(`BUILD_OPENCV',
ARG OPENCV_REPO=https://github.com/opencv/opencv/archive/OPENCV_VER.tar.gz
RUN cd BUILD_HOME && \
  wget -O - ${OPENCV_REPO} | tar xz
# TODO: file a bug against opencv since they do not accept full libdir
RUN cd BUILD_HOME/opencv-OPENCV_VER && mkdir build && cd build && \
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=BUILD_PREFIX \
    -DCMAKE_INSTALL_LIBDIR=patsubst(BUILD_LIBDIR,BUILD_PREFIX/) \
    -DOPENCV_GENERATE_PKGCONFIG=ON \
    -DBUILD_DOCS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_TESTS=OFF \
    -DWITH_OPENEXR=ifelse(OPENCV_OPENEXR,false,OFF,ON) \
    -DWITH_OPENJPEG=ifelse(OPENCV_OPENJPEG,true,ON,OFF) \
    -DWITH_GSTREAMER=ifelse(OPENCV_GSTREAMER,true,ON,OFF) \
    -DWITH_JASPER=ifelse(OPENCV_JASPER,true,ON,OFF) \
    .. && \
  make -j "$(nproc)" && \
  make install DESTDIR=BUILD_DESTDIR && \
  make install
)

REG(OPENCV)

include(end.m4)dnl
