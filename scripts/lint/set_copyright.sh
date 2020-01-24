#!/bin/bash
#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

python_interpreter="python3"

if [[ "$OSTYPE" == "msys" ]]; then
    python_interpreter="python"
fi

converter=$(dirname $(readlink -f $0))/set_copyright.py

if [ "${1:-STAGED}" = "HEAD" ]; then
    git diff-tree --no-commit-id --name-only -r HEAD | xargs -n 1 $python_interpreter $converter
else
    git diff --cached --name-only | xargs -n 1 $python_interpreter $converter
    git diff --name-only | xargs -n 1 echo "Not scanned: "
fi
