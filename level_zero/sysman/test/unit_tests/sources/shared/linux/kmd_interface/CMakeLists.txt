#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SYSMAN_SHARED_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_kmd_interface_xe.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_kmd_interface_i915.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_kmd_interface_i915_prelim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_kmd_interface_i915_upstream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_kmd_interface_xe.cpp
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_SYSMAN_SHARED_TESTS}
  )
endif()
