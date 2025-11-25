#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE3P_CORE)
  set(NEO_CORE_HELPERS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}gfx_core_helper_xe3p_core_additional.inl
  )

  set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})
  add_subdirectories()
endif()
