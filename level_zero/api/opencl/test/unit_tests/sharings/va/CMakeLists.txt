#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX AND NEO__LIBVA_FOUND)
  set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
               ${CMAKE_CURRENT_SOURCE_DIR}/test_leo_va_device.cpp
  )
endif()
