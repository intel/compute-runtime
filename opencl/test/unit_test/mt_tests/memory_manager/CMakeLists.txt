#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_memory_manager
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_clear_queue_mt_tests.cpp

    # necessary dependencies from neo_shared_tests
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/memory_manager/deferred_deleter_mt_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_memory_manager})
