#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  if(NEO_ENABLE_i915_PRELIM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/debug_session.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/debug_session.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/drm_helper.cpp
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.cpp
    )
  endif()

endif()
