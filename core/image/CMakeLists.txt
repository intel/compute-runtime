#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_IMAGE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state.h
  ${CMAKE_CURRENT_SOURCE_DIR}/image_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/image_tgllp_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/image_skl_plus.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_IMAGE ${NEO_CORE_IMAGE})
add_subdirectories()
