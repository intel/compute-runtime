#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_sharings_gl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_dll_helper.h
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sharings_gl})
add_subdirectories()
