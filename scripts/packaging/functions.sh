#!/bin/bash

#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set -ex

get_api_version() {
    API_VERSION="${API_VERSION:-}"
    API_VERSION_SRC="${API_VERSION_SRC:-}"
    API_DEB_MODEL_LINK=""
    API_RPM_MODEL_LINK=""
    if [ "${COMPONENT_MODEL}" != "ci" ]; then
        API_DEB_MODEL_LINK="~${COMPONENT_MODEL:-unknown}${BUILD_ID:-0}"
        API_RPM_MODEL_LINK=".${COMPONENT_MODEL:-unknown}${BUILD_ID:-0}"
    fi
}
