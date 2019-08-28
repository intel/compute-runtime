#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_device
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_timers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_device_info_size_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_device_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sub_device_tests.cpp
)

if(WIN32)
  list(APPEND IGDRCL_SRCS_tests_device
    ${CMAKE_CURRENT_SOURCE_DIR}/device_win_timers_tests.cpp
  )
endif()
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_device})
add_subdirectories()
