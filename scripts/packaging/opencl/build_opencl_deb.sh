#!/usr/bin/env bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_DIR="$( cd "$( dirname "${DIR}/../../../../" )" && pwd )"

BUILD_DIR="${REPO_DIR}/../build_opencl"
SKIP_UNIT_TESTS=${SKIP_UNIT_TESTS:-FALSE}

BRANCH_SUFFIX="$( cat ${REPO_DIR}/.branch )"

ENABLE_OPENCL="${ENABLE_OPENCL:-1}"
if [ "${ENABLE_OPENCL}" == "0" ]; then
    exit 0
fi

LOG_CCACHE_STATS="${LOG_CCACHE_STATS:-0}"

export BUILD_ID="${BUILD_ID:-1}"
export CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
export DO_NOT_RUN_AUB_TESTS="${DO_NOT_RUN_AUB_TESTS:-FALSE}"

source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/functions.sh"
source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/opencl/opencl.sh"

get_api_version     # API_VERSION-API_VERSION_SRC and API_DEB_MODEL_LINK
get_opencl_version  # NEO_OCL_VERSION_MAJOR.NEO_OCL_VERSION_MINOR.NEO_OCL_VERSION_BUILD

export NEO_OCL_VERSION_MAJOR
export NEO_OCL_VERSION_MINOR
export NEO_OCL_VERSION_BUILD

VERSION="${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_OCL_VERSION_BUILD}.${API_VERSION}-${API_VERSION_SRC}${API_DEB_MODEL_LINK}"
PKG_VERSION=${VERSION}

if [ "${CMAKE_BUILD_TYPE}" != "Release" ]; then
    PKG_VERSION="${PKG_VERSION}+$(echo "$CMAKE_BUILD_TYPE" | tr '[:upper:]' '[:lower:]')1"
fi

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/debian

COPYRIGHT="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/opencl/${OS_TYPE}/copyright"

cp -pR ${REPO_DIR}/scripts/packaging/opencl/${OS_TYPE}/debian/* $BUILD_DIR/debian/
cp $COPYRIGHT $BUILD_DIR/debian/

#needs a top level CMAKE file
cat << EOF | tee $BUILD_DIR/CMakeLists.txt
cmake_minimum_required (VERSION 3.2 FATAL_ERROR)

project(igdrcl)

add_subdirectory($REPO_DIR opencl)
EOF

(
    cd $BUILD_DIR
    if [ "${LOG_CCACHE_STATS}" == "1" ]; then
        ccache -z
    fi
    export DEB_BUILD_OPTIONS="nodocs notest nocheck"
    export DH_VERBOSE=1
    if [ "${CMAKE_BUILD_TYPE}" != "Release" ]; then
      export DH_INTERNAL_BUILDFLAGS=1
    fi
    if [ "${ENABLE_ULT}" == "0" ]; then
        SKIP_UNIT_TESTS="TRUE"
    fi
    export SKIP_UNIT_TESTS

    dch -v ${PKG_VERSION} -m "build $PKG_VERSION" -b
    dpkg-buildpackage -j`nproc --all` -us -uc -b -rfakeroot
    sudo dpkg -i --force-depends ../*.deb
    if [ "${LOG_CCACHE_STATS}" == "1" ]; then
        ccache -s
        ccache -s | grep 'cache hit rate' | cut -d ' ' -f 4- | xargs -I{} echo OpenCL {} >> $REPO_DIR/../output/logs/ccache.log
    fi
)

mkdir -p ${REPO_DIR}/../output/dbgsym

mv ${REPO_DIR}/../*.deb ${REPO_DIR}/../output/
find ${REPO_DIR}/.. -maxdepth 1 -name \*.ddeb -type f -print0 | xargs -0r mv -t ${REPO_DIR}/../output/dbgsym/
