#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMPILER_INTERFACE_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/intermediate_representations_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linker_mock.h
  ${CMAKE_CURRENT_SOURCE_DIR}/linker_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_COMPILER_INTERFACE_TESTS ${NEO_CORE_COMPILER_INTERFACE_TESTS})
