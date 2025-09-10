#
# Copyright (C) 2021-2025 Intel Corporation
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

  if(NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/debug_session.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/prelim/debug_session.h
    )
  endif()

  if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/debug_session.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/${BRANCH_DIR_SUFFIX}handle_event_helper.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/${BRANCH_DIR_SUFFIX}debug_session_sip_external_lib.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/${BRANCH_DIR_SUFFIX}debug_session_sip_external_lib.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/xe/debug_session.h
    )
  endif()

  add_subdirectories()

endif()
