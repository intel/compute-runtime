#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_command_queue
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_test_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioq_task_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ooq_task_tests_mt.cpp

    # necessary dependencies from igdrcl_tests
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/enqueue_fixture.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_command_queue})
