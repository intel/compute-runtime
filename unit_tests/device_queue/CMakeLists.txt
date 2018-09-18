#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_device_queue
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue_hw_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_device_queue_info_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_device_queue})
