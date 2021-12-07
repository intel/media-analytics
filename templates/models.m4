dnl # Copyright (c) 2020 Intel Corporation
dnl #
dnl # Permission is hereby granted, free of charge, to any person obtaining a copy
dnl # of this software and associated documentation files (the "Software"), to deal
dnl # in the Software without restriction, including without limitation the rights
dnl # to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
dnl # copies of the Software, and to permit persons to whom the Software is
dnl # furnished to do so, subject to the following conditions:
dnl #
dnl # The above copyright notice and this permission notice shall be included in all
dnl # copies or substantial portions of the Software.
dnl #
dnl # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl # IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
dnl # AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
dnl # OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
dnl # SOFTWARE.
dnl #
include(begin.m4)

DECLARE(`MODELS_REPO',https://github.com/dlstreamer/pipeline-zoo-models.git)
DECLARE(`MODELS_VER',ca51c02)

define(`GET_MODELS',`dnl
INSTALL_PKGS(`ca-certificates git git-lfs')

RUN mkdir -p BUILD_HOME && cd BUILD_HOME && \
  git clone MODELS_REPO && \
  cd pipeline-zoo-models && git checkout MODELS_VER')

define(`INSTALL_MODELS',`dnl
COPY --from=$1 BUILD_HOME/pipeline-zoo-models/storage/ssd_mobilenet_v1_coco_INT8 BUILD_PREFIX/models/ssd_mobilenet_v1_coco_INT8
COPY --from=$1 BUILD_HOME/pipeline-zoo-models/storage/resnet-50-tf_INT8 BUILD_PREFIX/models/resnet-50-tf_INT8
')

include(end.m4)
