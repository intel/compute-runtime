#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_SRCS_DLL_LINUX
       ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/linux/prelim/debug_session_linux_helper.cpp
  )
else()
  list(APPEND L0_SRCS_DLL_LINUX
       ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/linux/debug_session_linux_helper.cpp
  )
endif()

set_property(GLOBAL PROPERTY L0_SRCS_DLL_LINUX ${L0_SRCS_DLL_LINUX})
