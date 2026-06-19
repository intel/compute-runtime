#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBTRACEFS_FOUND)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_tracefs_api.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_tracefs_api.cpp
    )
  endif()
endif()

add_subdirectories()
