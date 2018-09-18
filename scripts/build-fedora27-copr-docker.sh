#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker info
docker build -f scripts/docker/Dockerfile-fedora-27-copr-gcc-7 -t neo-fedora-27-copr-gcc-7:ci .

