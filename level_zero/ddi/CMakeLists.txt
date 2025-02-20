#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_ddi_tables.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_ddi_tables.h
)

add_subdirectories()

