#!/bin/sh
#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

#
# Simple, file based tests for copyright script
# script return non-zero error code if something went wrong.
# diff output is printed
#

script_directory=$(dirname "$0")

python_interpreter="python3"

if [[ "$OSTYPE" == "msys" ]]; then
    python_interpreter="python"
fi

$python_interpreter "${script_directory}/../../lint/set_copyright.py" "${script_directory}"/in/*

for i in "${script_directory}"/in/*
do
    fn=$(basename $i)
    diff -du "${script_directory}/in/${fn}" "${script_directory}/out/${fn}"
done
