#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_enqueue_kernel
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_tests_mt.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_enqueue_kernel})
