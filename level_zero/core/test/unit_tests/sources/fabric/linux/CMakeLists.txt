#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND AND NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${TARGET_NAME} PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_fabric_iaf.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_fabric_multi_hop.cpp
    )
  endif()
endif()
