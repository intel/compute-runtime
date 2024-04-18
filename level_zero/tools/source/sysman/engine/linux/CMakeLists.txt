#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_engine_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_engine_imp_prelim.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_engine_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_engine_common.cpp
  )
endif()

