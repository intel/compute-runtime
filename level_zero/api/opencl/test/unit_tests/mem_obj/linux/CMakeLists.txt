#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
               ${CMAKE_CURRENT_SOURCE_DIR}/test_buffer_linux.cpp
  )
endif()
