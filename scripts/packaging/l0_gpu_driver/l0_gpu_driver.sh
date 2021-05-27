#!/usr/bin/env bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

get_l0_gpu_driver_version() {
    NEO_L0_VERSION_MAJOR=$(grep -m1 NEO_L0_VERSION_MAJOR ${REPO_DIR}/version.cmake | awk -F"MAJOR " '{ print $2 }' | awk -F")" '{ print $1 }')
    NEO_L0_VERSION_MINOR=$(grep -m1 NEO_L0_VERSION_MINOR ${REPO_DIR}/version.cmake | awk -F"MINOR " '{ print $2 }' | awk -F")" '{ print $1 }')
    NEO_TAG=$(git -C ${REPO_DIR} describe --abbrev=1 --tags | awk -F"." '{ nn=split($NF, nfa, "."); if(nn==2) {printf("%s-%s", nfa[1], nfa[2]);} else {print $NF;} }')
    NEO_L0_VERSION_PATCH=$(echo $NEO_TAG | awk -F '-' '{ print $1; }')
    NEO_L0_VERSION_HOTFIX=$(echo $NEO_TAG | awk -F '-' '{ if(NF>1) {printf(".%s",  $2);} }')
}
