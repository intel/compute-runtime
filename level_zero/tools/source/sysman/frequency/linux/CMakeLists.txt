#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  if(NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_frequency_imp_prelim.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_frequency_imp_prelim.h
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_frequency_imp.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_frequency_imp.h
    )
  endif()
endif()
