#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_helper.cpp
  )

  if(NEO_ENABLE_i915_PRELIM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/debug_session.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/debug_session.h
    )
  endif()

  if(NEO_ENABLE_XE_DRM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/debug_session.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/debug_session.h
    )
  endif()

  add_subdirectories()

endif()
