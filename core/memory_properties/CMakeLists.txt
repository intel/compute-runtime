#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_MEMORY_PROPERTIES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/memory_properties_flags.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_flags_common.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_MEMORY_PROPERTIES ${NEO_CORE_MEMORY_PROPERTIES})
add_subdirectories()