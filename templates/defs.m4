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
divert(-1)

define(`BUILD_PREFIX',/opt/intel/samples)
define(`BUILD_WHEEL',/opt/wheel)
define(`CLEANUP_MAN',no)
define(`DLDT_WARNING_AS_ERRORS',ON)
define(`DLDT_WITH_MKL',false)
define(`DLDT_WITH_OPENCL',true)
dnl define(`DLDT_PATCH_DIR',patches/dldt-ie)
define(`OPENCV_EIGEN',false)
define(`OPENCV_GSTREAMER',false)

define(`ECHO_SEP',` \
    ')

define(`WHEEL_EXTRAS',`ifelse($1,`',,`patsubst($1,` +',``,'')')')

divert(0)dnl
