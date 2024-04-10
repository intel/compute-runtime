#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_temperature.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_temperature.cpp
  )
endif()
add_subdirectories()
