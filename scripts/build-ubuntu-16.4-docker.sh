#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker build -f scripts/docker/Dockerfile-ubuntu-16.04-gcc-5 -t neo-ubuntu-16.04-gcc-5:ci .

