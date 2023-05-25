#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_scheduler_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_scheduler_imp.cpp
  )
endif()
