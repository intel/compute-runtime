#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_ras_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_ras_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_ras_imp_hbm.cpp
  )

  if(LIBGENL_FOUND)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/sysman_os_ras_imp_gt.cpp
    )

  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_ras_imp_gt.cpp
    )
  endif()
endif()

add_subdirectories()
