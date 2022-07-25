#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC_AND_LATER)
  target_sources(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_pvc_and_later.inl)
endif()

add_subdirectories()
