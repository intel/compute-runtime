#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
endif()
