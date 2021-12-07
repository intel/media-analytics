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

DECLARE(`DLDT_VER',dev_11102021)
DECLARE(`DLDT_REPO_URL',https://github.com/openvinotoolkit/openvino.git)
DECLARE(`DLDT_WARNING_AS_ERRORS',false)
DECLARE(`DLDT_WITH_OPENCL',false)
DECLARE(`DLDT_WITH_MKL',true)
dnl MO=Model Optimizer
DECLARE(`DLDT_WITH_MO',false)

define(`DLDT_WITH_OPENCL_BUILD',dnl
ifelse(DLDT_WITH_OPENCL,true,`ifelse(
OS_NAME,ubuntu,ocl-icd-opencl-dev opencl-headers)'))dnl

define(`DLDT_WITH_OPENCL_INSTALL',dnl
ifelse(DLDT_WITH_OPENCL,true,`ifelse(
OS_NAME,ubuntu,ocl-icd-libopencl1)'))dnl

define(`DLDT_MO_BUILD',`dnl
ifelse(DLDT_WITH_MO,true,python3 python3-pip cython3 patchelf python3-setuptools python3-wheel)')

define(`DLDT_MO_INSTALL',`dnl
ifelse(DLDT_WITH_MO,true,python3 python3-pip)')

ifelse(OS_NAME,ubuntu,dnl
define(`DLDT_BUILD_DEPS',`ca-certificates cmake gcc g++ git dnl
  libboost-all-dev libgtk2.0-dev libgtk-3-dev libtool libusb-1.0-0-dev dnl
  make patch python python-yaml xz-utils dnl
  DLDT_MO_BUILD DLDT_WITH_OPENCL_BUILD')
define(`DLDT_INSTALL_DEPS',`libgtk-3-0 DLDT_MO_INSTALL DLDT_WITH_OPENCL_INSTALL')
)

define(`dldt_extras',`')

define(`BUILD_DLDT',
ARG DLDT_REPO=DLDT_REPO_URL

RUN git clone ${DLDT_REPO} BUILD_HOME/openvino && \
  cd BUILD_HOME/openvino && \
  git checkout DLDT_VER && \
  git submodule update --init --recursive
ifdef(`DLDT_PATCH_DIR',dnl
`PATCH(BUILD_HOME/openvino,DLDT_PATCH_DIR)'
)
RUN cd BUILD_HOME/openvino && \
ifelse(DLDT_WARNING_AS_ERRORS,false,`dnl
  sed -i s/-Werror//g $(grep -ril Werror inference-engine/thirdparty/) && \
')dnl
  mkdir build && cd build && \
  cmake \
    -DCMAKE_INSTALL_PREFIX=BUILD_PREFIX/dldt \
    -DENABLE_CPPLINT=OFF \
    -DENABLE_GNA=OFF \
    -DENABLE_VPU=OFF \
    -DENABLE_OPENCV=OFF \
    -DENABLE_MKL_DNN=ifelse(DLDT_WITH_MKL,true,ON,OFF) \
    -DENABLE_CLDNN=ifelse(DLDT_WITH_OPENCL,false,OFF,ON) \
    -DENABLE_PYTHON=ifelse(DLDT_WITH_MO,true,ON,OFF) \
    -DENABLE_SAMPLES=OFF \
    -DENABLE_TESTS=OFF \
    -DBUILD_TESTS=OFF \
    -DTREAT_WARNING_AS_ERROR=ifelse(DLDT_WARNING_AS_ERRORS,false,OFF,ON) \
    -DNGRAPH_WARNINGS_AS_ERRORS=ifelse(DLDT_WARNING_AS_ERRORS,false,OFF,ON) \
    -DNGRAPH_COMPONENT_PREFIX=deployment_tools/ngraph/ \
    -DNGRAPH_UNIT_TEST_ENABLE=OFF \
    -DNGRAPH_TEST_UTIL_ENABLE=OFF \
    .. && \
  make -j $(nproc) && \
  make install && \
  make install DESTDIR=BUILD_DESTDIR
ifelse(DLDT_WITH_MO,true,`
RUN cd BUILD_HOME/openvino/model-optimizer && \
  python3 setup.py build && \
  python3 setup.py bdist_wheel --dist-dir=BUILD_WHEEL
')dnl

ARG CUSTOM_DLDT_INSTALL_DIR=BUILD_PREFIX/dldt
ARG CUSTOM_IE_DIR=${CUSTOM_DLDT_INSTALL_DIR}/runtime
ARG CUSTOM_IE_LIBDIR=${CUSTOM_IE_DIR}/lib/intel64
ENV CUSTOM_DLDT=${CUSTOM_IE_DIR}

ENV OpenVINO_DIR=${CUSTOM_IE_DIR}/cmake
ENV InferenceEngine_DIR=${CUSTOM_IE_DIR}/cmake
ENV TBB_DIR=${CUSTOM_IE_DIR}/3rdparty/tbb/cmake
ENV ngraph_DIR=${CUSTOM_IE_DIR}/cmake

# Remove stuff we don't need
RUN cd BUILD_DESTDIR/BUILD_PREFIX/dldt && \
  rm -rf install_dependencies samples tools
)

define(`INSTALL_DLDT',
ARG CUSTOM_DLDT_INSTALL_DIR=BUILD_PREFIX/dldt
ARG CUSTOM_IE_DIR=${CUSTOM_DLDT_INSTALL_DIR}/runtime
ARG CUSTOM_IE_LIBDIR=${CUSTOM_IE_DIR}/lib/intel64
RUN { \
   echo "${CUSTOM_IE_LIBDIR}"; \
   echo "${CUSTOM_IE_DIR}/3rdparty/tbb/lib"; \
} > /etc/ld.so.conf.d/dldt.conf
RUN ldconfig
ifelse(DLDT_WITH_MO,true,`
COPY --from=build BUILD_WHEEL/openvino_mo* BUILD_WHEEL/
RUN pip3 install --prefix=BUILD_PREFIX --find-links=BUILD_WHEEL openvino_mo WHEEL_EXTRAS(dldt_extras)
ENV PYTHONPATH="$CUSTOM_DLDT_INSTALL_DIR/python/python3.8:${PYTHONPATH}"
')dnl
)

REG(DLDT)

include(end.m4)dnl
