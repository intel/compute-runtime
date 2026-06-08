#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${TARGET_NAME} PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/debug_api_linux_mt_tests.cpp
    )
  endif()
endif()
