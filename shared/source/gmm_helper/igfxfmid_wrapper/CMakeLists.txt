#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GMM_HELPER_IGFXFMID_WRAPPER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}neo_igfxfmid.h
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_GMM_HELPER ${NEO_CORE_GMM_HELPER_IGFXFMID_WRAPPER})
add_subdirectories()
