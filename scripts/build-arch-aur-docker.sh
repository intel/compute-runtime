#!/bin/bash
#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker build -f scripts/docker/Dockerfile-arch-aur-gcc-8 -t neo-arch-aur-gcc-8:ci .

