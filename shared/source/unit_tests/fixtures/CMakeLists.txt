#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_tests_fixtures
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/command_container_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_tests_fixtures ${NEO_CORE_tests_fixtures})
