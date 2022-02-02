#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_xe_hpg_core_dg2_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_windows})
  add_subdirectories()
endif()
