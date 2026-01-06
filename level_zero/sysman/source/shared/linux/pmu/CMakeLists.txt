#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmu_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmu_imp_xe.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmu_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmu.h
  )
endif()
