#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(L0_SOURCES_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/registry_path.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/driver_teardown.cpp
  )
  set_property(GLOBAL PROPERTY L0_SOURCES_LINUX ${L0_SOURCES_LINUX})
endif()
