#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_compiler_interface
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/binary_cache_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface_tests.cpp
)

get_property(NEO_CORE_COMPILER_INTERFACE_TESTS GLOBAL PROPERTY NEO_CORE_COMPILER_INTERFACE_TESTS)
list(APPEND IGDRCL_SRCS_tests_compiler_interface ${NEO_CORE_COMPILER_INTERFACE_TESTS})

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_compiler_interface})
