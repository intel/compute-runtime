#!/bin/bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -e

clang-format-11 --version

INPUT_PATH="${INPUT_PATH:-.}"
INPUT_IREGEX="${INPUT_IREGEX:-.*\.(cpp|h|inl)}"

(
    cd ${INPUT_PATH}
    git fetch origin ${GITHUB_BASE_REF}
    set -x
    git diff -U0 --no-color origin/master..HEAD | clang-format-diff-11 -p1 -i -v -iregex ${INPUT_IREGEX}
    set +x
)

if [ -n "$(git -C ${INPUT_PATH} status --porcelain)" ]; then
    git -C ${INPUT_PATH} diff
    exit 1
fi
