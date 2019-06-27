#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_linux_dll_tests_gen11_ehl
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/device_id_tests_ehl.cpp
)
target_sources(igdrcl_linux_dll_tests PRIVATE ${IGDRCL_SRCS_linux_dll_tests_gen11_ehl})
