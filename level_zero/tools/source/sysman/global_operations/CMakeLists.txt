#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/global_operations.h
               ${CMAKE_CURRENT_SOURCE_DIR}/global_operations_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/global_operations_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_global_operations.h
)

add_subdirectories()
