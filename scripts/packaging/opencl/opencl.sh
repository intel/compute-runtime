#!/usr/bin/env bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

get_opencl_version() {
    commit_time=$(git -C ${REPO_DIR} show -s --format=%ct)
    commit_ww=($(${REPO_DIR}/scripts/neo_ww_calculator.py ${commit_time}))
    date_m=${commit_ww[1]}
    NEO_OCL_VERSION_MAJOR=$(echo $commit_ww | awk -F '.' '{print $1;}')
    NEO_OCL_VERSION_MINOR=$(echo $commit_ww | awk -F '.' '{print $2;}')
    NEO_OCL_TAG=$(git -C ${REPO_DIR} describe --abbrev=1 --tags | awk -F"." '{ nn=split($NF, nfa, "."); if(nn==2) {printf("%s-%s", nfa[1], nfa[2]);} else {print $NF;} }')
    NEO_OCL_VERSION_BUILD=$(echo $NEO_OCL_TAG | awk -F '-' '{ print $1; }')
    NEO_OCL_VERSION_HOTFIX=$(echo $NEO_OCL_TAG | awk -F '-' '{ if(NF>1) {printf(".%s",  $2);} }')
}
