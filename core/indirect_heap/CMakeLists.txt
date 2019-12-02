#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_INDIRECT_HEAP
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/indirect_heap.h
)

set_property(GLOBAL PROPERTY NEO_CORE_INDIRECT_HEAP ${NEO_CORE_INDIRECT_HEAP})
add_subdirectories()
