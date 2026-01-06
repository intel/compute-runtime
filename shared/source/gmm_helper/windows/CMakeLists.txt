#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GMM_HELPER_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}gmm_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_memory_base.cpp
)

add_subdirectories()
set_property(GLOBAL APPEND PROPERTY NEO_CORE_GMM_HELPER_WINDOWS ${NEO_CORE_GMM_HELPER_WINDOWS})
