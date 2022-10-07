#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND AND NEO_ENABLE_i915_PRELIM_DETECTION)
    target_sources(${TARGET_NAME} PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_fabric_iaf.cpp
    )
  endif()
endif()