#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

wget https://aur.archlinux.org/cgit/aur.git/snapshot/ncurses5-compat-libs.tar.gz
tar -xzf ncurses5-compat-libs.tar.gz
pushd ncurses5-compat-libs
makepkg --skippgpcheck -i --noconfirm
popd

