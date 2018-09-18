#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_command_queue
  # local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

  # necessary dependencies from igdrcl_tests
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/enqueue_api_tests_mt_with_asyncGPU.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/enqueue_kernel_mt_tests.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/enqueue_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/ooq_task_tests_mt.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/ioq_task_tests_mt.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_command_queue})
