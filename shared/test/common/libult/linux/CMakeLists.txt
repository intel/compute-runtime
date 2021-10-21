#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock.h
  )
  target_sources(neo_libult PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/directory_linux.cpp
  )

  add_subdirectories()
endif()
