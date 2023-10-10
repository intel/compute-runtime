#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_memory.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_memory.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_memory_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_memory_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_memory.h
)

add_subdirectories()
