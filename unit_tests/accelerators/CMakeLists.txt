#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_accelerators
   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
   ${CMAKE_CURRENT_SOURCE_DIR}/media_image_arg_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_accelerators})
