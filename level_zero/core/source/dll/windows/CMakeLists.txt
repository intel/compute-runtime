#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  if(L0_USE_INTERNAL_EXPORTS)
    set(ZE_INTEL_GPU64_EXPORTS_DEF ${CMAKE_CURRENT_SOURCE_DIR}/ZeIntelGpu64InternalExports.def)
  else()
    set(ZE_INTEL_GPU64_EXPORTS_DEF ${CMAKE_CURRENT_SOURCE_DIR}/ZeIntelGpu64ReleaseExports.def)
  endif()

  target_sources(${TARGET_NAME_L0}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${ZE_INTEL_GPU64_EXPORTS_DEF}
                 ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/windows/debug_session_windows_helper.cpp
  )
endif()
