#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen8_bdw_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_bdw.cpp
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen8_bdw_linux})
  add_subdirectory(dll)
endif()
