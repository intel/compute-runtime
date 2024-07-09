#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE2_HPG_CORE)
  set(RUNTIME_SRCS_XE2_HPG_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_XE2_HPG_CORE})
  set_property(GLOBAL PROPERTY RUNTIME_SRCS_XE2_HPG_CORE ${RUNTIME_SRCS_XE2_HPG_CORE})
endif()

