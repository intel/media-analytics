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

define(`VA_SAMPLE_BUILD_DEPS',`cmake g++ libigfxcmrt-dev libmfx-dev libva-dev make ocl-icd-opencl-dev opencl-headers pkg-config')
define(`VA_SAMPLE_INSTALL_DEPS',`intel-media-va-driver-non-free intel-opencl-icd libigfxcmrt7 libmfx1 ifdef(`LIBMFXGEN1',LIBMFXGEN1) libva-drm2')

pushdef(`CFLAGS',`-Werror')

define(`BUILD_VA_SAMPLE',
COPY va_sample BUILD_HOME/va_sample
RUN cd BUILD_HOME/va_sample \
  && mkdir build && cd build \
  && cmake \
    -DCMAKE_INSTALL_PREFIX=BUILD_PREFIX \
    -DCMAKE_INSTALL_LIBDIR=BUILD_LIBDIR \
    -DCMAKE_C_FLAGS="CFLAGS" \
    -DCMAKE_CXX_FLAGS="CFLAGS" \
    .. \
  && make VERBOSE=1 -j $(nproc --all) \
  && make install DESTDIR=BUILD_DESTDIR
)

popdef(`CFLAGS')

REG(VA_SAMPLE)

include(end.m4)
