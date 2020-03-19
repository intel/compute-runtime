#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/encode_compute_mode_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/encode_compute_mode_tgllp_plus.inl
)
add_subdirectories()
set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_CONTAINER ${NEO_CORE_COMMAND_CONTAINER})
