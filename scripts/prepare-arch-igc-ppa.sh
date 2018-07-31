#!/bin/bash
# Copyright (c) 2018, Intel Corporation
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

IGC_VER=18.30.707-2

cd /root
mkdir /root/igc

install_libs() {
    wget https://launchpad.net/~intel-opencl/+archive/ubuntu/intel-opencl/+files/$1
    ar -x $1
    tar -xJf data.tar.xz
    rm -f data.tar.xz $1
}

install_libs intel-opencl-clang_4.0.15-1~ppa1~bionic1_amd64.deb
install_libs intel-igc-core_${IGC_VER}~ppa1~bionic1_amd64.deb
install_libs intel-igc-opencl-dev_${IGC_VER}~ppa1~bionic1_amd64.deb
install_libs intel-igc-opencl_${IGC_VER}~ppa1~bionic1_amd64.deb

cp -ar usr /
ln -sf /usr/lib/x86_64-linux-gnu/*.so /usr/lib64
ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/*.pc /usr/lib64/pkgconfig
mkdir /root/build

