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

DECLARE(`OMZ_VER',2022.1.0)
DECLARE(`OMZ_SRC_REPO',https://github.com/opencv/open_model_zoo/archive/OMZ_VER.tar.gz)

DECLARE(`OMZ_CAFFE2',false)     # ~850GB
DECLARE(`OMZ_PYTORCH',false)    # ~850GB
DECLARE(`OMZ_TENSORFLOW',true) # ~400GB

define(`omz_extras',`')

ifelse(OMZ_CAFFE2,true,`APPEND_TO_DEF(`omz_extras',caffe2)')
ifelse(OMZ_PYTORCH,true,`APPEND_TO_DEF(`omz_extras',torch)')
ifelse(OMZ_TENSORFLOW,true,`APPEND_TO_DEF(`omz_extras',tensorflow)')

define(`OMZ_BUILD_DEPS',`python3 python3-pip python3-setuptools python3-wheel wget')
define(`OMZ_INSTALL_DEPS',`python3 python3-pip python3-requests python3-yaml')

define(`BUILD_OMZ',`dnl
ARG OMZ_REPO=OMZ_SRC_REPO
RUN cd BUILD_HOME && \
  wget -O - ${OMZ_REPO} | tar xz

RUN cd BUILD_HOME/open_model_zoo-OMZ_VER/tools/model_tools && \
  python3 setup.py build && \
  python3 setup.py bdist_wheel --dist-dir=BUILD_WHEEL
')

define(`INSTALL_OMZ',`dnl
COPY --from=$2 BUILD_WHEEL/omz_tools* BUILD_WHEEL/
RUN pip3 install --prefix=BUILD_PREFIX --find-links=BUILD_WHEEL omz_tools WHEEL_EXTRAS(omz_extras)

VOLUME BUILD_PREFIX/models
')

REG(OMZ)

include(end.m4)
