#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_device_gl
    ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_gl_tests.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_device_gl})
endif()
