#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_sharings_gl
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sharings_gl})
add_subdirectories()
