#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS_SIP_EXTERNAL_LIB
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}sip_external_lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sip_external_lib.h
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS_SIP_EXTERNAL_LIB ${NEO_CORE_HELPERS_SIP_EXTERNAL_LIB})

add_subdirectories()
