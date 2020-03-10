#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen12lp_tgllp_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_tgllp_linux})
  add_subdirectories()
endif()
