#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_command_queue
  # local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

  # necessary dependencies from igdrcl_tests
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/enqueue_kernel_mt_tests.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/enqueue_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/ooq_task_tests_mt.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/ioq_task_tests_mt.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_command_queue})
