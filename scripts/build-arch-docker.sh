#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker build -f scripts/docker/Dockerfile-arch-ppa-gcc-8 -t neo-arch-ppa-gcc-8:ci .

