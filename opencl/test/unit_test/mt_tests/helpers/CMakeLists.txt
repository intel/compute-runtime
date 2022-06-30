#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_helpers
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/interlocked_max_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/map_operations_handler_mt_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_helpers})
