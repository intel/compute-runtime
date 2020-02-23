#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(IGDRCL_SRCS_tests_helpers_linux
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_linux_tests.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers_linux})
  add_subdirectories()
  set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_helpers_linux ${IGDRCL_SRCS_tests_helpers_linux})
endif()
