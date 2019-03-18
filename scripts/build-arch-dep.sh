#!/bin/bash
#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

wget https://aur.archlinux.org/cgit/aur.git/snapshot/opencl-clang-git.tar.gz
tar -xzf opencl-clang-git.tar.gz
cd opencl-clang-git
makepkg -i --noconfirm
cd ..

wget https://aur.archlinux.org/cgit/aur.git/snapshot/intel-graphics-compiler.tar.gz
tar -xzf intel-graphics-compiler.tar.gz
cd intel-graphics-compiler
makepkg -i --noconfirm
cd ..
