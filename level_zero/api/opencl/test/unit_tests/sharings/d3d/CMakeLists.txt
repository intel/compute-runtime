#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
               ${CMAKE_CURRENT_SOURCE_DIR}/test_leo_d3d_texture.cpp
  )
endif()
