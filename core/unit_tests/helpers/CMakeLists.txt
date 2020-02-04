#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager_state_restore.h
  ${CMAKE_CURRENT_SOURCE_DIR}/default_hw_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/default_hw_info.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hash_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_management.h
  ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/string_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/string_to_hash_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_hw_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_hw_config.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_hw_helper.h
)

set_property(GLOBAL PROPERTY NEO_CORE_HELPERS_TESTS ${NEO_CORE_HELPERS_TESTS})
add_subdirectories()
