#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface.h
  )
endif()
