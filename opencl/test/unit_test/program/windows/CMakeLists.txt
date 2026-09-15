#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_program_windows
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/program_required_libs_windows_tests.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_program_windows})
  set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_program_windows ${IGDRCL_SRCS_tests_program_windows})
endif()
