#!/usr/bin/env bash

#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_DIR="$( cd "$( dirname "${DIR}/../../../" )" && pwd )"

BUILD_DIR="${REPO_DIR}/../build_neo"
NEO_SKIP_UNIT_TESTS=${NEO_SKIP_UNIT_TESTS:-FALSE}
NEO_SKIP_AUB_TESTS_RUN=${NEO_SKIP_AUB_TESTS_RUN:-TRUE}
NEO_DISABLE_BUILTINS_COMPILATION=${NEO_DISABLE_BUILTINS_COMPILATION:-FALSE}
NEO_LEGACY_PLATFORMS_SUPPORT=${NEO_LEGACY_PLATFORMS_SUPPORT:-FALSE}
NEO_CURRENT_PLATFORMS_SUPPORT=${NEO_CURRENT_PLATFORMS_SUPPORT:-TRUE}
NEO_BUILD_WITH_L0=${NEO_BUILD_WITH_L0:-TRUE}

BRANCH_SUFFIX="$( cat ${REPO_DIR}/.branch )"

ENABLE_NEO="${ENABLE_NEO:-1}"
if [ "${ENABLE_NEO}" == "0" ]; then
    exit 0
fi

LOG_CCACHE_STATS="${LOG_CCACHE_STATS:-0}"

export BUILD_ID="${BUILD_ID:-1}"
export CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
export NEO_SKIP_AUB_TESTS_RUN="${NEO_SKIP_AUB_TESTS_RUN:-TRUE}"

source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/functions.sh"
source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/versions.sh"

get_api_version # API_VERSION-API_VERSION_SRC and API_DEB_MODEL_LINK
get_neo_version # NEO_VERSION_MAJOR.NEO_VERSION_MINOR.NEO_VERSION_BUILD.NEO_VERSION_HOTFIX

export NEO_VERSION_MAJOR
export NEO_VERSION_MINOR
export NEO_VERSION_BUILD
export NEO_VERSION_HOTFIX

if [ -z "${BRANCH_SUFFIX}" ]; then
    VERSION="${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${NEO_VERSION_HOTFIX}${API_DEB_MODEL_LINK}"
else
    VERSION="1:${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}${API_VERSION}-${NEO_VERSION_HOTFIX}${API_VERSION_SRC}${API_DEB_MODEL_LINK}"
fi

PKG_VERSION=${VERSION}

if [ "${CMAKE_BUILD_TYPE}" != "Release" ]; then
    PKG_VERSION="${PKG_VERSION}+$(echo "$CMAKE_BUILD_TYPE" | tr '[:upper:]' '[:lower:]')1"
fi

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/debian

COPYRIGHT="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/ubuntu/copyright"
CONTROL="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/ubuntu/control"
SHLIBS="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/ubuntu/shlibs.local"

cp -pR ${REPO_DIR}/scripts/packaging/ubuntu/debian/* $BUILD_DIR/debian/
cp $COPYRIGHT $BUILD_DIR/debian/
cp $CONTROL $BUILD_DIR/debian/
if [ -f "${SHLIBS}" ]; then
    cp $SHLIBS $BUILD_DIR/debian/
fi

if [ "${NEO_BUILD_WITH_L0}" != "TRUE" ]; then
    rm $BUILD_DIR/debian/libze-intel-gpu1.install
    rm $BUILD_DIR/debian/libze-intel-gpu-dev.install
fi

LEVEL_ZERO_DEVEL_NAME=${LEVEL_ZERO_DEVEL_NAME:-level-zero-devel}

LEVEL_ZERO_DEVEL_VERSION=$(apt-cache policy ${LEVEL_ZERO_DEVEL_NAME} | grep Installed | cut -f2- -d ':' | xargs)
if [ ! -z "${LEVEL_ZERO_DEVEL_VERSION}" ]; then
    perl -pi -e "s/^ level-zero-devel(?=,|$)/ ${LEVEL_ZERO_DEVEL_NAME} (=$LEVEL_ZERO_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
fi

if [ -z "${BRANCH_SUFFIX}" ]; then
    GMM_VERSION=$(apt-cache policy libigdgmm12 | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${GMM_VERSION}" ]; then
        perl -pi -e "s/^ libigdgmm12(?=,|$)/ libigdgmm12 (>=$GMM_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    GMM_DEVEL_VERSION=$(apt-cache policy libigdgmm-dev | grep Installed | cut -f2- -d ':' | xargs)
    if [ ! -z "${GMM_DEVEL_VERSION}" ]; then
        perl -pi -e "s/^ libigdgmm-dev(?=,|$)/ libigdgmm-dev (>=$GMM_DEVEL_VERSION)/" "$BUILD_DIR/debian/control"
    fi
    IGC_CORE_VERSION_LOWER=$(apt-cache policy intel-igc-core-2 | grep Installed | cut -f2- -d ':' | cut -f1-2 -d'.' | xargs)
    if [ ! -z "${IGC_CORE_VERSION_LOWER}" ]; then
        IGC_CORE_VERSION_MAJOR="${IGC_CORE_VERSION_LOWER%%.*}"
        IGC_CORE_VERSION_MINOR="${IGC_CORE_VERSION_LOWER##*.}"
        IGC_CORE_VERSION_UPPER="${IGC_CORE_VERSION_MAJOR}.$((IGC_CORE_VERSION_MINOR + 3))"
        perl -pi -e "s/^ intel-igc-core-2(?=,|$)/ intel-igc-core-2 (>=$IGC_CORE_VERSION_LOWER), intel-igc-core-2 (<<$IGC_CORE_VERSION_UPPER)/" "$BUILD_DIR/debian/control"
    fi
    IGC_VERSION_LOWER=$(apt-cache policy intel-igc-opencl-2 | grep Installed | cut -f2- -d ':' | cut -f1-2 -d'.' | xargs)
    if [ ! -z "${IGC_VERSION_LOWER}" ]; then
        IGC_VERSION_MAJOR="${IGC_VERSION_LOWER%%.*}"
        IGC_VERSION_MINOR="${IGC_VERSION_LOWER##*.}"
        IGC_VERSION_UPPER="${IGC_VERSION_MAJOR}.$((IGC_VERSION_MINOR + 3))"
        perl -pi -e "s/^ intel-igc-opencl-2(?=,|$)/ intel-igc-opencl-2 (>=$IGC_VERSION_LOWER), intel-igc-opencl-2 (<<$IGC_VERSION_UPPER)/" "$BUILD_DIR/debian/control"
    fi
    IGC_DEVEL_VERSION_LOWER=$(apt-cache policy intel-igc-opencl-devel | grep Installed | cut -f2- -d ':' | cut -f1-2 -d'.' | xargs)
    if [ ! -z "${IGC_DEVEL_VERSION_LOWER}" ]; then
        IGC_DEVEL_VERSION_MAJOR="${IGC_DEVEL_VERSION_LOWER%%.*}"
        IGC_DEVEL_VERSION_MINOR="${IGC_DEVEL_VERSION_LOWER##*.}"
        IGC_DEVEL_VERSION_UPPER="${IGC_DEVEL_VERSION_MAJOR}.$((IGC_DEVEL_VERSION_MINOR + 3))"
        perl -pi -e "s/^ intel-igc-opencl-devel(?=,|$)/ intel-igc-opencl-devel (>=$IGC_DEVEL_VERSION_LOWER), intel-igc-opencl-devel (<<$IGC_DEVEL_VERSION_UPPER)/" "$BUILD_DIR/debian/control"
    fi
fi

echo "NEO_CURRENT_PLATFORMS_SUPPORT: ${NEO_CURRENT_PLATFORMS_SUPPORT}"
echo "NEO_LEGACY_PLATFORMS_SUPPORT: ${NEO_LEGACY_PLATFORMS_SUPPORT}"

if [[ "${NEO_LEGACY_PLATFORMS_SUPPORT}" == "TRUE" ]] && [[ ! "${NEO_CURRENT_PLATFORMS_SUPPORT}" == "TRUE" ]]; then
    echo "Building Legacy package"
    export NEO_OCLOC_VERSION_MODE=0
    export NEO_LEGACY_VERSION=legacy1
    perl -pi -e "s/^Package: intel-opencl-icd$/Package: intel-opencl-icd-${NEO_LEGACY_VERSION}/" "$BUILD_DIR/debian/control"
    mv -v "$BUILD_DIR/debian/intel-opencl-icd.install" "$BUILD_DIR/debian/intel-opencl-icd-${NEO_LEGACY_VERSION}.install"

    perl -pi -e "s/^Package: intel-ocloc$/Package: intel-ocloc-${NEO_LEGACY_VERSION}/" "$BUILD_DIR/debian/control"
    mv -v "$BUILD_DIR/debian/intel-ocloc.install" "$BUILD_DIR/debian/intel-ocloc-${NEO_LEGACY_VERSION}.install" 
    mv -v "$BUILD_DIR/debian/intel-ocloc.postinst" "$BUILD_DIR/debian/intel-ocloc-${NEO_LEGACY_VERSION}.postinst" 
    mv -v "$BUILD_DIR/debian/intel-ocloc.prerm" "$BUILD_DIR/debian/intel-ocloc-${NEO_LEGACY_VERSION}.prerm" 
    perl -pi -e "s/\/ocloc 0$/\/ocloc-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_OCLOC_VERSION_MODE} ${NEO_VERSION_MAJOR}${NEO_VERSION_MINOR}${NEO_OCLOC_VERSION_MODE}/" "$BUILD_DIR/debian/intel-ocloc-${NEO_LEGACY_VERSION}.postinst"
    perl -pi -e "s/\/ocloc$/\/ocloc-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_OCLOC_VERSION_MODE}/" "$BUILD_DIR/debian/intel-ocloc-${NEO_LEGACY_VERSION}.prerm"
    perl -i -lne 'if (/^Package: intel-ocloc-dev$/ .. /^$/) { print if !$flag } else {$flag=1; print}' "$BUILD_DIR/debian/control"

    if [ "${NEO_BUILD_WITH_L0}" == "TRUE" ]; then
        perl -pi -e "s/^Package: libze-intel-gpu1$/Package: libze-intel-gpu1-${NEO_LEGACY_VERSION}/" "$BUILD_DIR/debian/control"
        mv -v "$BUILD_DIR/debian/libze-intel-gpu1.install" "$BUILD_DIR/debian/libze-intel-gpu1-${NEO_LEGACY_VERSION}.install" 
    fi
    perl -i -lne 'if (/^Package: libze-intel-gpu-dev$/ .. /^$/) { print if !$flag } else {$flag=1; print}' "$BUILD_DIR/debian/control"
else
    echo "Building Current/Full package"
    export NEO_OCLOC_VERSION_MODE=1

    perl -pi -e "s/\/ocloc 0$/\/ocloc-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_OCLOC_VERSION_MODE} ${NEO_VERSION_MAJOR}${NEO_VERSION_MINOR}${NEO_OCLOC_VERSION_MODE}/" "$BUILD_DIR/debian/intel-ocloc.postinst"
    perl -pi -e "s/\/ocloc$/\/ocloc-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_OCLOC_VERSION_MODE}/" "$BUILD_DIR/debian/intel-ocloc.prerm"
fi


#needs a top level CMAKE file
cat << EOF | tee $BUILD_DIR/CMakeLists.txt
cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

project(neo)

add_subdirectory($REPO_DIR neo)
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
        NEO_SKIP_UNIT_TESTS="TRUE"
    fi
    if [ "${TARGET_ARCH}" == "aarch64" ]; then
        NEO_SKIP_UNIT_TESTS="TRUE"
        export NEO_DISABLE_BUILTINS_COMPILATION="TRUE"
    fi
    export NEO_DISABLE_BUILTINS_COMPILATION
    export NEO_SKIP_UNIT_TESTS

    dch -v ${PKG_VERSION} -m "build $PKG_VERSION"
    ulimit -n 65535 || true
    dpkg-buildpackage -j`nproc --all` -us -uc -b -rfakeroot

    if [ "${NEO_BUILD_WITH_L0}" != "TRUE" ]; then
        rm -f ${REPO_DIR}/../libze-intel-gpu1_*.deb
        rm -f ${REPO_DIR}/../libze-intel-gpu1-dbgsym_*.ddeb
        rm -f ${REPO_DIR}/../libze-intel-gpu-dev_*.deb
    fi

    sudo dpkg -i --force-depends ../*.deb
    if [ "${LOG_CCACHE_STATS}" == "1" ]; then
        ccache -s
        ccache -s | grep 'cache hit rate' | cut -d ' ' -f 4- | xargs -I{} echo Neo {} >> $REPO_DIR/../output/logs/ccache.log
    fi
)

mkdir -p ${REPO_DIR}/../output/dbgsym

mv ${REPO_DIR}/../*.deb ${REPO_DIR}/../output/
find ${REPO_DIR}/.. -maxdepth 1 -name \*.ddeb -type f -print0 | xargs -0r mv -t ${REPO_DIR}/../output/dbgsym/
