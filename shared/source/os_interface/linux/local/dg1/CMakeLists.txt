#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG1 AND "${BRANCH_TYPE}" STREQUAL "")
  set(NEO_CORE_OS_INTERFACE_LINUX_LOCAL_DG1
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_helper_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/drm_tip_helper.cpp
  )
  set(CORE_SRCS_LINK_LINUX_LOCAL_DG1
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_local_memory_helper_dg1.cpp
  )
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX_LOCAL_DG1})
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_SRCS_LINK ${CORE_SRCS_LINK_LINUX_LOCAL_DG1})
endif()
