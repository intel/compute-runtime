#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_fixtures_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_allocator_multi_device_fixture_linux.cpp
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_fixtures_linux})
endif()
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_fixtures_linux ${IGDRCL_SRCS_tests_fixtures_linux})
add_subdirectories()
