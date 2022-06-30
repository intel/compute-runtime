#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(L0_SOURCES_WINDOWS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/registry_path.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/driver_teardown.cpp
  )
  set_property(GLOBAL PROPERTY L0_SOURCES_WINDOWS ${L0_SOURCES_WINDOWS})
endif()
