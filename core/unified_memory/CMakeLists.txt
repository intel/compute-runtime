#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_UNIFIED_MEMORY
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/usm_memory_support.h
)

set_property(GLOBAL PROPERTY NEO_UNIFIED_MEMORY ${NEO_UNIFIED_MEMORY})
