#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen12_dg1_linux
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_dg1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12_dg1_linux})
  add_subdirectory(dll)
endif()
