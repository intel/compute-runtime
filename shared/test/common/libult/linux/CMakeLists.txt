#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock.h
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}drm_mock_engine.cpp
  )

  if(NEO_ENABLE_i915_PRELIM_DETECTION)
    target_sources(neo_libult_common PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/drm_query_mock.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/drm_query_mock.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_prelim_context.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_prelim_context.cpp
    )
  endif()

  target_sources(neo_libult PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/directory_linux.cpp
  )

  add_subdirectories()
endif()
