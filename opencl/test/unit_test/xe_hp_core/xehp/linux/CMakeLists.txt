#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_xe_hp_core_xehp_linux
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hp_core_xehp_linux})
  add_subdirectory(dll)
endif()
