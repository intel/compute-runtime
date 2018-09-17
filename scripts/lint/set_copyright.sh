#!/bin/bash
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

converter=$(dirname $(readlink -f $0))/set_copyright.py

if [ "${1:-STAGED}" = "HEAD" ]; then
    git diff-tree --no-commit-id --name-only -r HEAD | xargs -n 1 python $converter
else
    git diff --cached --name-only | xargs -n 1 python $converter
    git diff --name-only | xargs -n 1 echo "Not scanned: "
fi
