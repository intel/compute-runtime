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
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp_prelim.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp_prelim.h
    )
  elseif(SUPPORT_DG1 AND "${BRANCH_TYPE}" STREQUAL "")
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp_dg1.cpp
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp.cpp
    )
  endif()

endif()
