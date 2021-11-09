#!/bin/bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker login -u "$DOCKER_USERNAME" -p "$DOCKER_PASSWORD"
docker build -f scripts/docker/Dockerfile-fedora-34-copr -t neo-fedora-34-copr:ci .

