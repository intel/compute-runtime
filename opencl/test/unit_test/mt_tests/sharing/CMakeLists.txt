#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_sharing
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_mt_tests.cpp
)
if(WIN32)
  target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_sharing})
endif()

