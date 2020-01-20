#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  list (APPEND IGDRCL_SRCS_tests_mocks_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_arb_sync_event_windows.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_sharing_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_sharing_windows.h
  )
  target_sources(igdrcl_mocks PRIVATE ${IGDRCL_SRCS_tests_mocks_windows})
endif()

