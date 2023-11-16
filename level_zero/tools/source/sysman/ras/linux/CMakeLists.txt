#
# Copyright (C) 2020-2023 Intel Corporation
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
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp_prelim.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp_prelim.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp_gt.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp_hbm.cpp
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp.h
    )
  endif()
endif()
