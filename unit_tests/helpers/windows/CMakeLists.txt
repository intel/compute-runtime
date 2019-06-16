#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_helpers_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_windows_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_function.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_function.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers_windows})
  add_subdirectories()
endif()
