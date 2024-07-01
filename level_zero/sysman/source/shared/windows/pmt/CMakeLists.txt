#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmt_helper.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmt.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pmt.h
  )
endif()