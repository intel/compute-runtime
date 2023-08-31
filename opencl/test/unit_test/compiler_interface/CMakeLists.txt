#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_compiler_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_compiler_interface_tests.cpp
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_compiler_interface})

if(NOT WIN32)
  target_sources(igdrcl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/linux/default_cl_cache_config_tests.cpp)
else()
  target_sources(igdrcl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/windows/default_cl_cache_config_tests.cpp)
endif()
