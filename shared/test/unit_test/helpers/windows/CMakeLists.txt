#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/gl_helper_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_windows_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_function.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_function.cpp
  )

  add_subdirectories()
endif()
