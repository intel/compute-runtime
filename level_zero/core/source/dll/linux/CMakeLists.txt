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

    if(NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM)
      target_sources(${TARGET_NAME_L0}
                     PRIVATE
                     ${NEO_SOURCE_DIR}/shared/source/os_interface/linux/xe/eudebug/enable_eudebug_interface_upstream.cpp
      )
    endif()
    if(NEO_ENABLE_XE_PRELIM_DETECTION)
      target_sources(${TARGET_NAME_L0}
                     PRIVATE
                     ${NEO_SOURCE_DIR}/shared/source/os_interface/linux/xe/eudebug/enable_eudebug_interface_prelim.cpp
      )
    endif()
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

