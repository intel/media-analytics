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

# formerly https://github.com/opencv/gst-video-analytics
DECLARE(`GVA_REPO_URL',https://github.com/openvinotoolkit/dlstreamer_gst.git)
DECLARE(`GVA_VER',v1.5.2)
DECLARE(`GVA_ENABLE_PAHO_INST',OFF)
DECLARE(`GVA_ENABLE_RDKAFKA_INST',OFF)
DECLARE(`GVA_ENABLE_VAS_TRACKER',OFF)

DECLARE(`GST_VAAPI_WITH_GVA_PATCH',true)
DECLARE(`GST_VAAPI_GVA_PATCH_VER',GVA_VER)
DECLARE(`GST_VAAPI_GVA_PATCH_URL',https://raw.githubusercontent.com/openvinotoolkit/dlstreamer_gst/${GSTREAMER_VAAPI_PATCH_VER}/patches/gstreamer-vaapi/vasurface_qdata.patch)

include(gst-plugins-base.m4)
include(gst-vaapi.m4)

define(`GVA_BUILD_DEPS',cmake git ocl-icd-opencl-dev opencl-headers pkg-config dnl
  python3 python3-dev python-gi-dev python3-setuptools python3-wheel)

# TODO: bug in DL Streamer, it tries to load dev symlinks, thus libglib2.0-dev is needed
define(`GVA_INSTALL_DEPS',`python3-pip python3-gst-1.0 libglib2.0-dev')

define(`BUILD_GVA',
ARG GVA_REPO=GVA_REPO_URL

RUN git clone ${GVA_REPO} BUILD_HOME/gst-video-analytics && \
  cd BUILD_HOME/gst-video-analytics && \
  git checkout GVA_VER && \
  git submodule update --init

RUN mkdir -p BUILD_HOME/gst-video-analytics/build && \
  cd BUILD_HOME/gst-video-analytics/build && \
  cmake \
    -DCMAKE_INSTALL_PREFIX=BUILD_PREFIX \
    -DENABLE_SAMPLES=OFF \
    -DENABLE_TESTS=OFF \
    -DENABLE_PAHO_INSTALLATION=GVA_ENABLE_PAHO_INST \
    -DENABLE_RDKAFKA_INSTALLATION=GVA_ENABLE_RDKAFKA_INST \
    -DENABLE_VAAPI=ON \
    -DENABLE_VAS_TRACKER=GVA_ENABLE_VAS_TRACKER \
    .. && \
  make -j $(nproc) && \
  make install DESTDIR=BUILD_DESTDIR

COPY assets/gva/setup.py BUILD_HOME/gst-video-analytics/
RUN cd BUILD_HOME/gst-video-analytics/ && \
  python3 setup.py build && \
  python3 setup.py bdist_wheel --dist-dir=BUILD_WHEEL
)

define(`INSTALL_GVA',
COPY --from=$2 BUILD_WHEEL BUILD_WHEEL
RUN python3 -m pip install --prefix=BUILD_PREFIX BUILD_WHEEL/* && rm -rf /opt/wheel
)

REG(GVA)

include(end.m4)
