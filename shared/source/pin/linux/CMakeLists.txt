#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  list(APPEND NEO_GTPIN
       ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
       ${CMAKE_CURRENT_SOURCE_DIR}/os_pin.h
  )
  include_directories(${NEO_SHARED_DIRECTORY}/pin/)
endif()