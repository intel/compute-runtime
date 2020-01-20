#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_command_queue_gl_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_gl_tests_windows.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_command_queue_gl_windows})
endif()
