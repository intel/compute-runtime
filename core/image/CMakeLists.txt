#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_IMAGE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state.h
)

set_property(GLOBAL PROPERTY NEO_CORE_IMAGE ${NEO_CORE_IMAGE})
add_subdirectories()
