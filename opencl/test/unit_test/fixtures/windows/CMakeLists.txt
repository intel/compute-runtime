#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_fixtures_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_allocator_multi_device_fixture_windows.cpp
)
if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_fixtures_windows})
endif()
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_fixtures_windows ${IGDRCL_SRCS_tests_fixtures_windows})
add_subdirectories()
