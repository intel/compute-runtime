#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DEBUG_SETTINGS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_variables_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/debug_variables.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/translate_debug_settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions/translate_debug_settings.h
)

set_property(GLOBAL PROPERTY NEO_CORE_DEBUG_SETTINGS ${NEO_CORE_DEBUG_SETTINGS})
