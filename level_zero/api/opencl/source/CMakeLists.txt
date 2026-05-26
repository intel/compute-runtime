#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_include_directories(${L0_STATIC_LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sharings/gl/windows/include)
else()
  target_include_directories(${L0_STATIC_LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sharings/gl/linux/include)
endif()

add_subdirectories()
