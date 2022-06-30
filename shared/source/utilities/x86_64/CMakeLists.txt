#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_UTILITIES
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info_x86_64.cpp
  )
endif()
