#!/bin/bash -x
#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

IMAGE=neo-${BUILD_OS}-${COMPILER}:ci
export WORKFLOW="${WORKFLOW:-VERIFY}"
export BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "Using ${IMAGE}"
echo "Build type: ${BUILD_TYPE}"
echo "Workflow: ${WORKFLOW}"

rm -rf build
mkdir build

skip_unit_tests="FALSE"
if [ "${WORKFLOW}" = "CI" ]; then
    skip_unit_tests="TRUE"
    if [ "${COMPILER}" = "gcc-5" ] || [ "${COMPILER}" == "gcc-6" ]; then
        skip_unit_tests="FALSE"
    fi
fi
docker run --rm -u `id -u`:`id -g` -v `pwd`:/src -w /src/build -t ${IMAGE} cmake -GNinja ../neo -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DSKIP_UNIT_TESTS=${skip_unit_tests} -DDO_NOT_RUN_AUB_TESTS=TRUE
docker run --rm -u `id -u`:`id -g` -v `pwd`:/src -w /src/build -t ${IMAGE} ninja package
