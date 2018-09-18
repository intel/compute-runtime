#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

DOCKERFILE=Dockerfile-${BUILD_OS}-${COMPILER}
IMAGE=neo-${BUILD_OS}-${COMPILER}:ci

if [ -n "$GEN" ]
then
    DOCKERFILE=${DOCKERFILE}-${GEN}
    IMAGE=neo-${BUILD_OS}-${COMPILER}-${GEN}:ci
fi

git clone ../compute-runtime neo && \
docker build -f scripts/docker/${DOCKERFILE} -t ${IMAGE} . && \
docker images
