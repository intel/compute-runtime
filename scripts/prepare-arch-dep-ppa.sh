#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

cd /root
IGC_VER="`grep igc_version_required neo/scripts/fedora.spec.in | grep global | awk '{print $3}'`-1"
GMM_VER="`grep gmmlib_version_required neo/scripts/fedora.spec.in | grep global | awk '{print $3}'`-1"

echo "IGC_VER=${IGC_VER}"
echo "GMM_VER=${GMM_VER}"

install_libs() {
    wget https://launchpad.net/~ocl-dev/+archive/ubuntu/intel-opencl/+files/$1
    ar -x $1
    tar -xJf data.tar.xz
    rm -f data.tar.xz $1
}

install_libs intel-opencl-clang_4.0.16-1~ppa1~bionic1_amd64.deb
install_libs intel-igc-core_${IGC_VER}~ppa1~bionic1_amd64.deb
install_libs intel-igc-opencl-dev_${IGC_VER}~ppa1~bionic1_amd64.deb
install_libs intel-igc-opencl_${IGC_VER}~ppa1~bionic1_amd64.deb
install_libs intel-gmmlib_${GMM_VER}~ppa1~bionic1_amd64.deb
install_libs intel-gmmlib-dev_${GMM_VER}~ppa1~bionic1_amd64.deb

cp -ar usr /
ln -sf /usr/lib/x86_64-linux-gnu/*.so* /usr/lib64
ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/*.pc /usr/lib64/pkgconfig
mkdir /root/build
