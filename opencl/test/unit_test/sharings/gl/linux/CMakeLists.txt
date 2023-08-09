#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(IGDRCL_SRCS_tests_sharings_gl_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/gl_arb_sync_event_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gl_library_name.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing_enable_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gl_types_tests.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sharings_gl_linux})
endif()
