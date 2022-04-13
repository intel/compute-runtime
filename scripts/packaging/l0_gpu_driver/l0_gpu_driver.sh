#!/usr/bin/env bash

#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

get_l0_gpu_driver_version() {
    __NEO_L0_VERSION_MAJOR_TMP=$(grep -m1 NEO_L0_VERSION_MAJOR ${REPO_DIR}/version.cmake | awk -F"MAJOR " '{ print $2 }' | awk -F")" '{ print $1 }')
    NEO_L0_VERSION_MAJOR="${NEO_L0_VERSION_MAJOR:-$__NEO_L0_VERSION_MAJOR_TMP}"
    unset __NEO_L0_VERSION_MAJOR_TMP
    __NEO_L0_VERSION_MINOR_TMP=$(grep -m1 NEO_L0_VERSION_MINOR ${REPO_DIR}/version.cmake | awk -F"MINOR " '{ print $2 }' | awk -F")" '{ print $1 }')
    NEO_L0_VERSION_MINOR="${NEO_L0_VERSION_MINOR:-$__NEO_L0_VERSION_MINOR_TMP}"
    unset __NEO_L0_VERSION_MINOR_TMP
    __NEO_TAG_TMP=$(git -C ${REPO_DIR} describe --abbrev=1 --tags | awk -F"." '{ nn=split($NF, nfa, "."); if(nn==2) {printf("%s-%s", nfa[1], nfa[2]);} else {print $NF;} }')
    NEO_TAG="${NEO_TAG:-$__NEO_TAG_TMP}"
    unset __NEO_TAG_TMP
    __NEO_L0_VERSION_PATCH_TMP=$(echo $NEO_TAG | awk -F '-' '{ print $1; }' | sed 's/^0*//')
    NEO_L0_VERSION_PATCH="${NEO_L0_VERSION_PATCH:-$__NEO_L0_VERSION_PATCH_TMP}"
    unset __NEO_L0_VERSION_PATCH_TMP
    __NEO_L0_VERSION_HOTFIX_TMP=$(echo $NEO_TAG | awk -F '-' '{ if(NF>1) { print $2; } }')
    NEO_L0_VERSION_HOTFIX="${NEO_L0_VERSION_HOTFIX:-$__NEO_L0_VERSION_HOTFIX_TMP}"
    unset __NEO_L0_VERSION_HOTFIX_TMP
}
