#!/bin/bash
#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

DOCKERFILE=Dockerfile-${BUILD_OS}
IMAGE=neo-${BUILD_OS}:ci

if [ -n "$GEN" ]
then
    DOCKERFILE=${DOCKERFILE}-${GEN}
    IMAGE=neo-${BUILD_OS}-${GEN}:ci
fi

echo Using ${DOCKERFILE}

cd docker
docker build -f ${DOCKERFILE} -t ${IMAGE} .
docker images
