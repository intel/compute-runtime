#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_os_interface_windows_gl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_os_sharing_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_dll_helper.h
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_os_interface_windows_gl})
endif()
