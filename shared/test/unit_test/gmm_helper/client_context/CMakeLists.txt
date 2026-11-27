#
# Copyright (C) 2025-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}gmm_client_context_initialize_tests.cpp
)

if(DRIVER_MODEL STREQUAL "drm_or_wddm")
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}gmm_client_context_initialize_drm_or_wddm_tests.cpp
  )
endif()

add_subdirectories()
