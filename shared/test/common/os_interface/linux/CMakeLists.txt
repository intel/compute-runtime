#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_cache_info.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_device_blob.h
  )
  add_subdirectories()
endif()
