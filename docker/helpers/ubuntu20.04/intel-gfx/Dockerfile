# Copyright (c) 2021 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

ARG IMAGE=ubuntu:20.04
FROM $IMAGE AS base

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    curl dpkg-dev ca-certificates gpg-agent software-properties-common && \
  rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://repositories.intel.com/graphics/intel-graphics.key | apt-key add -
RUN apt-add-repository "deb https://repositories.intel.com/graphics/ubuntu focal main"

RUN apt-get update && apt-get install -y -d \
  intel-media-va-driver-non-free \
  intel-opencl-icd \
  libdrm-dev \
  libigfxcmrt7 \
  libigfxcmrt-dev \
  libmfx1 \
  libmfx-dev \
  libva2 \
  libva-drm2 \
  libva-x11-2 \
  libva-wayland2 \
  libva-dev \
  vainfo \
    >/tmp/downloaded.txt 2>&1

RUN cat /tmp/downloaded.txt | grep "repositories.intel.com" | cut -f5 -d\ >/tmp/list.txt;
RUN mkdir /opt/apt-pkgs && while read p; do cp /var/cache/apt/archives/$p*.deb /opt/apt-pkgs/; done </tmp/list.txt
RUN cd /opt/apt-pkgs && dpkg-scanpackages . > Packages
