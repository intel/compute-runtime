#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/path_linux_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_reader_tests.cpp
  )
endif()
