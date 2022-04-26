#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(IGDRCL_SRCS_tests_mem_obj_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/buffer_linux_tests.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_mem_obj_linux})
  set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_mem_obj_linux ${IGDRCL_SRCS_tests_mem_obj_linux})
endif()
