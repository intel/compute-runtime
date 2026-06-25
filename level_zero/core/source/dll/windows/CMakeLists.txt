#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME_L0}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/ZeIntelGpu64Exports.def
                 ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/windows/debug_session_windows_helper.cpp
  )
endif()
