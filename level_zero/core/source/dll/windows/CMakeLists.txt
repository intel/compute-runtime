#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME_L0}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/windows/debug_session_windows_helper.cpp
  )
endif()
