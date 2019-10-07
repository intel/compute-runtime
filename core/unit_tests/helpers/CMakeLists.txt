#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager_state_restore.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_management.h
)

set_property(GLOBAL PROPERTY NEO_CORE_HELPERS_TESTS ${NEO_CORE_HELPERS_TESTS})
add_subdirectories()
