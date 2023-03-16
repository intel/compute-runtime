#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/performance.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/performance.h
               ${CMAKE_CURRENT_SOURCE_DIR}/performance_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/performance_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_performance.h
)

add_subdirectories()
