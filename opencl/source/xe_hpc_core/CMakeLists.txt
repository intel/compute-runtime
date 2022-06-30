#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HPC_CORE)
  set(RUNTIME_SRCS_XE_HPC_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_XE_HPC_CORE})
  set_property(GLOBAL PROPERTY RUNTIME_SRCS_XE_HPC_CORE ${RUNTIME_SRCS_XE_HPC_CORE})
endif()
