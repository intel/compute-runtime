#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git tag -l

VER=`git describe --tags --abbrev=0`
IGC_INFO=($(git show ${VER}:../../manifests/manifest.yml | grep -U -A 1  intelgraphicscompiler ))
IGC_REV=${IGC_INFO[3]}

echo "NEO release: ${VER}"
echo "IGC_REV: ${IGC_REV}"

NEO_TOP_DIR=`git rev-parse --show-toplevel`
WRK_DIR=${NEO_TOP_DIR}/..
mkdir -p ${WRK_DIR}/igc/inc
cp CMakeLists.txt ${WRK_DIR}/igc
pushd ${WRK_DIR}/igc

wget https://github.com/intel/compute-runtime/releases/download/${VER}/intel-opencl_${VER}_amd64.deb
ar -x intel-opencl_${VER}_amd64.deb
tar -xJf data.tar.xz
find . -name libigdrcl.so -exec rm {} \;
LIB_PATH=`find . -name libigc.so`
echo "libraries found in ${LIB_PATH}"
ln -sv `dirname ${LIB_PATH}` lib

git clone https://github.com/intel/intel-graphics-compiler igc
pushd igc; git checkout ${IGC_REV}; popd

pushd inc
ln -s ../igc/IGC/AdaptorOCL/cif/cif cif
ln -s ../igc/IGC/AdaptorOCL/ocl_igc_interface ocl_igc_interface
ln -s ../igc/IGC/AdaptorOCL/ocl_igc_shared/device_enqueue/DeviceEnqueueInternalTypes.h DeviceEnqueueInternalTypes.h
ln -s ../igc/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_g7.h patch_g7.h
ln -s ../igc/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h patch_list.h
ln -s ../igc/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_shared.h patch_shared.h
ln -s ../igc/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h program_debug_data.h
popd

popd
