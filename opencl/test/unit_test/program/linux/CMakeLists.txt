#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(IGDRCL_SRCS_tests_program_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/program_required_libs_linux_tests.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_program_linux})
  set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_program_linux ${IGDRCL_SRCS_tests_program_linux})
endif()
