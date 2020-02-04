#!/bin/bash
#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

pwd
git -C neo diff -U0 --no-color --relative origin/master..HEAD | clang-format-diff-11 -p1 -i
git -C neo status
