#!/bin/bash

#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -e
set -x

mkdir /root/build
pacman -Suy --noconfirm clang cmake git make pkg-config ninja libva \
        intel-gmmlib intel-opencl-clang spirv-llvm-translator intel-graphics-compiler

