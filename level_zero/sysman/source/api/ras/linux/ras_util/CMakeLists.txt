#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/sysman_ras_util.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util_pmu.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util_gsc.cpp
  )

  if(LIBGENL_FOUND)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util_netlink.cpp

    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util_netlink_stub.cpp
    )
  endif()

  add_subdirectories()
endif()