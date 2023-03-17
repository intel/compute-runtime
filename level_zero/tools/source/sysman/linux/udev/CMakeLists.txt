#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(LIBUDEV_FOUND)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib.h
  )
else()
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib_imp_stub.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib.h
  )
endif()
