#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_scenarios_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_write_buffer_scenarios_windows_tests.cpp
)
if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_scenarios_windows})
endif()