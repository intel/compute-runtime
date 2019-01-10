#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen11_lkf_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_lkf.cpp
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_lkf_linux})
  add_subdirectory(dll)
endif()
