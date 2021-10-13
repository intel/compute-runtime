#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HP_SDV AND "${BRANCH_TYPE}" STREQUAL "")
  set(NEO_CORE_OS_INTERFACE_LINUX_LOCAL_XE_HP_SDV
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_helper_xe_hp_sdv.cpp
  )
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX_LOCAL_XE_HP_SDV})
endif()
