#!/usr/bin/env bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_DIR="$( cd "$( dirname "${DIR}/../../../../" )" && pwd )"

BUILD_DIR="${REPO_DIR}/../build_l0_gpu_driver"
SKIP_UNIT_TESTS=${SKIP_UNIT_TESTS:-FALSE}

BRANCH_SUFFIX="$( cat ${REPO_DIR}/.branch )"

ENABLE_L0_GPU_DRIVER="${ENABLE_L0_GPU_DRIVER:-1}"
if [ "${ENABLE_L0_GPU_DRIVER}" == "0" ]; then
    exit 0
fi

LOG_CCACHE_STATS="${LOG_CCACHE_STATS:-0}"

export BUILD_ID="${BUILD_ID:-1}"
export CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/functions.sh"
source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/l0_gpu_driver/l0_gpu_driver.sh"

get_api_version     # API_VERSION-API_VERSION_SRC and API_DEB_MODEL_LINK
get_l0_gpu_driver_version	# NEO_L0_VERSION_MAJOR.NEO_L0_VERSION_MINOR.NEO_L0_VERSION_PATCH

if [ -z "${BRANCH_SUFFIX}" ]; then
    VERSION="${NEO_L0_VERSION_MAJOR}.${NEO_L0_VERSION_MINOR}.${NEO_L0_VERSION_PATCH}${API_DEB_MODEL_LINK}"
else
    VERSION="${NEO_L0_VERSION_MAJOR}.${NEO_L0_VERSION_MINOR}.${NEO_L0_VERSION_PATCH}.${API_VERSION}-${API_VERSION_SRC}${API_DEB_MODEL_LINK}"
fi

PKG_VERSION=${VERSION}

if [ "${CMAKE_BUILD_TYPE}" != "Release" ]; then
    PKG_VERSION="${PKG_VERSION}+$(echo "$CMAKE_BUILD_TYPE" | tr '[:upper:]' '[:lower:]')1"
fi

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/debian

COPYRIGHT="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/l0_gpu_driver/${OS_TYPE}/copyright"
CONTROL="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/l0_gpu_driver/${OS_TYPE}/control"
SHLIBS="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/l0_gpu_driver/${OS_TYPE}/shlibs.local"

cp -pR ${REPO_DIR}/scripts/packaging/l0_gpu_driver/${OS_TYPE}/debian/* $BUILD_DIR/debian/
cp $COPYRIGHT $BUILD_DIR/debian/
cp $CONTROL $BUILD_DIR/debian/
if [ -f "${SHLIBS}" ]; then
    cp $SHLIBS $BUILD_DIR/debian/
fi

LEVEL_ZERO_DEVEL_VERSION=$(apt-cache policy level-zero-devel | grep Installed | cut -f2- -d ':' | xargs)
if [ ! -z "${LEVEL_ZERO_DEVEL_VERSION}" ]; then
    perl -pi -e "s/^ level-zero-devel(?=,|$)/ level-zero-devel (=$LEVEL_ZERO_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
fi

if [ -z "${BRANCH_SUFFIX}" ]; then
    GMM_VERSION=$(apt-cache policy intel-gmmlib | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${GMM_VERSION}" ]; then
        perl -pi -e "s/^ intel-gmmlib(?=,|$)/ intel-gmmlib (=$GMM_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    GMM_DEVEL_VERSION=$(apt-cache policy intel-gmmlib-devel | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${GMM_DEVEL_VERSION}" ]; then
        perl -pi -e "s/^ intel-gmmlib-devel(?=,|$)/ intel-gmmlib-devel (=$GMM_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
    fi

    IGC_VERSION=$(apt-cache policy intel-igc-opencl | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${IGC_VERSION}" ]; then
        perl -pi -e "s/^ intel-igc-opencl(?=,|$)/ intel-igc-opencl (=$IGC_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    IGC_DEVEL_VERSION=$(apt-cache policy intel-igc-opencl-devel | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${IGC_DEVEL_VERSION}" ]; then
        perl -pi -e "s/^ intel-igc-opencl-devel(?=,|$)/ intel-igc-opencl-devel (=$IGC_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
    fi
else
    GMM_VERSION=$(apt-cache policy libigdgmm11 | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${GMM_VERSION}" ]; then
        perl -pi -e "s/^ libigdgmm11(?=,|$)/ libigdgmm11 (=$GMM_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    GMM_DEVEL_VERSION=$(apt-cache policy libigdgmm-dev | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${GMM_DEVEL_VERSION}" ]; then
        perl -pi -e "s/^ libigdgmm-dev(?=,|$)/ libigdgmm-dev (=$GMM_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
    fi

    IGC_VERSION=$(apt-cache policy libigdfcl1 | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${IGC_VERSION}" ]; then
        perl -pi -e "s/^ libigdfcl1(?=,|$)/ libigdfcl1 (=$IGC_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    IGC_DEVEL_VERSION=$(apt-cache policy libigdfcl-dev | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${IGC_DEVEL_VERSION}" ]; then
        perl -pi -e "s/^ libigdfcl-dev(?=,|$)/ libigdfcl-dev (=$IGC_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    IGC_CORE_VERSION=$(apt-cache policy libigc1 | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${IGC_CORE_VERSION}" ]; then
        perl -pi -e "s/^ libigc1(?=,|$)/ libigc1 (=$IGC_CORE_VERSION)/" "$BUILD_DIR/debian/control"
    fi
fi

# Update rules file with new version
perl -pi -e "s/^ver = .*/ver = $NEO_L0_VERSION_PATCH/" $BUILD_DIR/debian/rules

#needs a top level CMAKE file
cat << EOF | tee $BUILD_DIR/CMakeLists.txt
cmake_minimum_required (VERSION 3.2 FATAL_ERROR)

project(l0_gpu_driver)

add_subdirectory($REPO_DIR l0_gpu_driver)
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

    dch -v ${PKG_VERSION} -m "build $PKG_VERSION"
    dpkg-buildpackage -j`nproc --all` -us -uc -b -rfakeroot
    sudo dpkg -i --force-depends ../*.deb
    if [ "${LOG_CCACHE_STATS}" == "1" ]; then
        ccache -s
        ccache -s | grep 'cache hit rate' | cut -d ' ' -f 4- | xargs -I{} echo LevelZero GPU Driver {} >> $REPO_DIR/../output/logs/ccache.log
    fi
)

mkdir -p ${REPO_DIR}/../output/dbgsym

mv ${REPO_DIR}/../*.deb ${REPO_DIR}/../output/
find ${REPO_DIR}/.. -maxdepth 1 -name \*.ddeb -type f -print0 | xargs -0r mv -t ${REPO_DIR}/../output/dbgsym/
