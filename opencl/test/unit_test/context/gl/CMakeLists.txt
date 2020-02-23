#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_context_gl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/context_gl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context_gl_tests.h
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_context_gl})
endif()
add_subdirectories()
