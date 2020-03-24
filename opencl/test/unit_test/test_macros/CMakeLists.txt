#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_test_macros
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/test_checks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_checks.h
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_test_macros})

get_property(NEO_CORE_test_macros GLOBAL PROPERTY NEO_CORE_test_macros)
list(APPEND IGDRCL_SRCS_test_macros
   ${NEO_CORE_test_macros}
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_test_macros})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_test_macros ${IGDRCL_SRCS_test_macros})