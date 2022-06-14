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

dnl v1.6 (2022.1) release 
DECLARE(`GVA_VER',v1.6)
DECLARE(`GST_VAAPI_GVA_PATCH_URL',dnl
dnl Commits from: https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/435
https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/commit/c27c158cb2efe285f6fcf909cb49e8a0ac88c568.patch dnl
https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/commit/7809c58664519e8af2266fbb1da8dd93e78b5bc3.patch)

include(gst-gva.m4)
include(gst-plugins-good.m4)

define(`GST_GVA_SAMPLE_INSTALL_DEPS',`intel-media-va-driver-non-free intel-opencl-icd libigfxcmrt7 libmfx1')

REG(GST_GVA_SAMPLE)

include(end.m4)
