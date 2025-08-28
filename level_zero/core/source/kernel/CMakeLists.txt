#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel.h
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_mutable_state.h
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_shared_state.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}kernel_imp_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sampler_patch_values.h
)
