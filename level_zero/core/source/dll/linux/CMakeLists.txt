#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/linux/xe/debug_session_xe_helper.cpp
    )
  endif()

  if(NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/linux/prelim/debug_session_linux_helper.cpp
    )
  else()
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/linux/debug_session_linux_helper.cpp
    )
  endif()
endif()

