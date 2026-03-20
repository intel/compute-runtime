#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_nl_api.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_iaf_nl_api.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_drm_nl_api.cpp
    )
  endif()
endif()

add_subdirectories()
