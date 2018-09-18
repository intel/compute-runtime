#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_context
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/context_get_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/context_multi_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/context_negative_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/context_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_diagnostics_enqueue_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_diagnostics_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_diagnostics_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/get_supported_image_formats_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_context})
add_subdirectories()
