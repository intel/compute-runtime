#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_IMAGE_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_IMAGE_TESTS ${NEO_CORE_IMAGE_TESTS})

add_subdirectories()