#!/bin/sh
#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

#
# Simple, file based tests for copyright script
# script return non-zero error code if something went wrong.
# diff output is printed
#

python ../../lint/set_copyright.py in/*

for i in in/*
do
    fn=$(basename $i)
    diff -du in/$fn out/$fn
done
