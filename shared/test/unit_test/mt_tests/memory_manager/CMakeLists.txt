#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_SRCS_mt_tests_memory_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_clear_queue_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_reuse_cleaner_tests_mt.cpp
)
target_sources(neo_shared_mt_tests PRIVATE ${NEO_SHARED_SRCS_mt_tests_memory_manager})
