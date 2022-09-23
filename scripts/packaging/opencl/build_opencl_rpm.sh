#!/usr/bin/env bash

#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_DIR="$( cd "$( dirname "${DIR}/../../../../" )" && pwd )"
BUILD_DIR="${REPO_DIR}/../build_opencl"

ENABLE_OPENCL="${ENABLE_OPENCL:-1}"
if [ "${ENABLE_OPENCL}" == "0" ]; then
    exit 0
fi

BUILD_SRPM="${BUILD_SRPM:-1}"
BUILD_RPM="${BUILD_RPM:-1}"
SPEC_FILE="${SPEC_FILE:-${OS_TYPE}}"
if [[ ${SPEC_FILE} == rhel* ]]; then
    SPEC_FILE=${SPEC_FILE%.*}
fi

export BUILD_ID="${BUILD_ID:-1}"
export CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

(
if [ "${BUILD_SRPM}" == "1" ]; then
    BRANCH_SUFFIX="$( cat ${REPO_DIR}/.branch )"
    PACKAGING_DIR="$REPO_DIR/scripts/packaging/opencl/${SPEC_FILE}"
    SPEC_SRC="$PACKAGING_DIR/SPECS/opencl.spec"
    SPEC="$BUILD_DIR/SPECS/opencl.spec"
    COPYRIGHT="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/opencl/${SPEC_FILE}/copyright"

    build_args=()
    if [ "${CMAKE_BUILD_TYPE}" == "Debug" ]; then
        export CFLAGS=" "
        export CXXFLAGS=" "
        export FFLAGS=" "
        build_args+=(--define 'name_suffix -debug')
    fi

    source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/functions.sh"
    source "${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/opencl/opencl.sh"

    get_opencl_version  # NEO_OCL_VERSION_MAJOR.NEO_OCL_VERSION_MINOR.NEO_OCL_VERSION_BUILD
    get_api_version     # API_VERSION-API_VERSION_SRC and API_RPM_MODEL_LINK

    VERSION="${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_OCL_VERSION_BUILD}${API_VERSION}"
    RELEASE="${NEO_OCL_VERSION_HOTFIX}${API_VERSION_SRC}${API_RPM_MODEL_LINK}"

    RELEASE_WITH_REGKEYS="${RELEASE_WITH_REGKEYS:-FALSE}"

    #setup rpm build tree
    rm -rf $BUILD_DIR
    mkdir -p $BUILD_DIR/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
    tar -c -I 'xz -6 -T0' -f $BUILD_DIR/SOURCES/compute-runtime-$VERSION.tar.xz -C $REPO_DIR --transform "s,${REPO_DIR:1},compute-runtime-$VERSION," --exclude=.git\* $REPO_DIR
    cp $COPYRIGHT $BUILD_DIR/SOURCES/
    cp $SPEC_SRC $BUILD_DIR/SPECS/

    PATCH_SPEC="${REPO_DIR}/scripts/packaging/${BRANCH_SUFFIX}/patch_spec.sh"

    if [ -f "$PATCH_SPEC" ]; then
        source "$PATCH_SPEC"
    fi
    if [ -z "${BRANCH_SUFFIX}" ]; then
        sed -i '/^Epoch: /d' ${SPEC}
    fi

    # Update spec file with new version
    perl -pi -e "s/^%global NEO_OCL_VERSION_MAJOR .*/%global NEO_OCL_VERSION_MAJOR ${NEO_OCL_VERSION_MAJOR}/" $BUILD_DIR/SPECS/opencl.spec
    perl -pi -e "s/^%global NEO_OCL_VERSION_MINOR .*/%global NEO_OCL_VERSION_MINOR ${NEO_OCL_VERSION_MINOR}/" $BUILD_DIR/SPECS/opencl.spec
    perl -pi -e "s/^%global NEO_OCL_VERSION_BUILD .*/%global NEO_OCL_VERSION_BUILD ${NEO_OCL_VERSION_BUILD}/" $BUILD_DIR/SPECS/opencl.spec
    perl -pi -e "s/^%global NEO_RELEASE_WITH_REGKEYS .*/%global NEO_RELEASE_WITH_REGKEYS ${RELEASE_WITH_REGKEYS}/" $BUILD_DIR/SPECS/opencl.spec
    perl -pi -e "s/^%global rel .*/%global rel ${RELEASE}/" $SPEC
    perl -pi -e "s/^%global ver .*/%global ver ${VERSION}/" $SPEC

    rpmbuild --define "_topdir $BUILD_DIR" -bs $SPEC --define 'build_type ${CMAKE_BUILD_TYPE}' "${build_args[@]}"
    mkdir -p ${REPO_DIR}/../output/SRPMS
    echo -n ${VERSION} > ${REPO_DIR}/../output/.opencl.version
    cp -v $BUILD_DIR/SRPMS/*.rpm ${REPO_DIR}/../output/SRPMS/
fi
)

if [ "${BUILD_RPM}" == "1" ]; then
  LOG_CCACHE_STATS="${LOG_CCACHE_STATS:-0}"

  rm -rf $BUILD_DIR
  mkdir -p $BUILD_DIR/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

  build_args=()
  build_args+=(--define "_topdir $BUILD_DIR")

  VERSION=$(cat ${REPO_DIR}/../output/.opencl.version)
  if [ "${LOG_CCACHE_STATS}" == "1" ]; then
    ccache -z
  fi
  export CCACHE_BASEDIR=$(readlink -m $BUILD_DIR/BUILD/compute-runtime-${VERSION}/)
  rpmbuild --rebuild ${REPO_DIR}/../output/SRPMS/intel-opencl-${VERSION}*.src.rpm "${build_args[@]}"
  if [ "${LOG_CCACHE_STATS}" == "1" ]; then
    ccache -s
    ccache -s | grep 'cache hit rate' | cut -d ' ' -f 4- | xargs -I{} echo OpenCL {} >> $REPO_DIR/../output/logs/ccache.log
  fi

  sudo rpm -Uvh --force $BUILD_DIR/RPMS/*/*.rpm
  cp $BUILD_DIR/RPMS/*/*.rpm $REPO_DIR/../output/
fi
