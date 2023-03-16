#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric.h
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_interface.h
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_mdfi.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric.cpp
)

add_subdirectories()
