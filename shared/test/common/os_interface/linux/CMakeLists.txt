#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_fixture.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_fixture.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_cache_info.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_device_blob.h
  )

  if(NEO_ENABLE_i915_PRELIM_DETECTION)
    target_sources(neo_libult_common PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_fixture_context.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_fixture_context.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_fixture_prelim.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_prelim_fixtures.h
    )
  endif()
endif()

add_subdirectories()