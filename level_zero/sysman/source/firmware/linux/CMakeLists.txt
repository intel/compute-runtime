#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_firmware_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_firmware_imp.h
  )

  if(NEO_ENABLE_i915_PRELIM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_firmware_imp_helper_prelim.cpp
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_firmware_imp_helper.cpp
    )
  endif()
endif()
