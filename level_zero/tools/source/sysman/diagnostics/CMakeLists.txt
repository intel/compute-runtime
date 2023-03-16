#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics.h
               ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_diagnostics.h
)

add_subdirectories()

