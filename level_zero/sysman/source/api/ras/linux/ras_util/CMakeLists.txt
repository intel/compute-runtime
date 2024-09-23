#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/sysman_ras_util.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util_pmu.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_util_gsc.cpp
  )
  add_subdirectories()
endif()