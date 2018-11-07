#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker info
docker build -f scripts/docker/Dockerfile-fedora-29-copr-gcc-8 -t neo-fedora-29-copr-gcc-8:ci .

