#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/command_container_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_CONTAINER_TESTS ${NEO_CORE_COMMAND_CONTAINER_TESTS})
add_subdirectories()