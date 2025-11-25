#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE3P_CORE)
  set(RUNTIME_SRCS_XE3P_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_hardware_commands_helper_xe3p.inl
  )

  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_XE3P_CORE})
  set_property(GLOBAL PROPERTY RUNTIME_SRCS_XE3P_CORE ${RUNTIME_SRCS_XE3P_CORE})
endif()

