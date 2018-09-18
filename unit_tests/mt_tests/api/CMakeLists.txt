#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_api
  # local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

  # necessary dependencies from igdrcl_tests
  ${IGDRCL_SOURCE_DIR}/unit_tests/api/cl_api_tests.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/api/cl_create_user_event_tests_mt.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/api/cl_get_platform_ids_tests_mt.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/api/cl_set_mem_object_destructor_callback_tests_mt.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_api})
