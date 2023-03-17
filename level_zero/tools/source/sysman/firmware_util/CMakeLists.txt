#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(igsc_FOUND)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/igsc_wrapper.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp_helper.cpp
  )
  add_subdirectories()
else()
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp_stub.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/igsc_wrapper.h
  )
endif()

