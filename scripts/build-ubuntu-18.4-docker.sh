#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

git fetch -t
git clone ../compute-runtime neo
docker build -f scripts/docker/Dockerfile-ubuntu-18.04-gcc-7 -t neo-ubuntu-18.4-gcc-7:ci .

