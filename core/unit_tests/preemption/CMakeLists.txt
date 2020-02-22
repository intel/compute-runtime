#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_SRCS_tests_preemption
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_tests_preemption ${NEO_CORE_SRCS_tests_preemption})
add_subdirectories()