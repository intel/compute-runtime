#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_aub_mem_dump_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_aub_alloc_dump_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_aub_mem_dump_tests})
