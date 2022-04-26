#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_mem_obj_windows
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/buffer_windows_tests.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_mem_obj_windows})
  set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_mem_obj_windows ${IGDRCL_SRCS_tests_mem_obj_windows})
endif()
