#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gmm_helper
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests_ocl.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gmm_helper})
add_subdirectories()
