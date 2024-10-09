#
# Copyright (C) 2021-2024 Intel Corporation
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
                   ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation_impl_prelim.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation_impl_prelim.h
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation_impl_upstream.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation_impl_upstream.h
    )
  endif()
endif()
