#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  list(APPEND IGDRCL_SRCS_tests_mocks_linux
       ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_arb_sync_event_linux.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_sharing_linux.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_sharing_linux.h
  )
  target_sources(igdrcl_mocks PRIVATE ${IGDRCL_SRCS_tests_mocks_linux})
endif()

