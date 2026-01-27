#
# Copyright (C) 2025-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GMM_HELPER_CLIENT_CONTEXT
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}gmm_client_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_client_context_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}gmm_client_context_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_client_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/map_gpu_va_gmm.h
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_GMM_HELPER ${NEO_CORE_GMM_HELPER_CLIENT_CONTEXT})
add_subdirectories()
