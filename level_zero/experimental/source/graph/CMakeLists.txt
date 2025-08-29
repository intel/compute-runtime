#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/graph.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/graph.h
               ${CMAKE_CURRENT_SOURCE_DIR}/graph_export.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/graph_export.h
               ${CMAKE_CURRENT_SOURCE_DIR}/captured_apis${BRANCH_DIR_SUFFIX}graph_captured_apis.h
               ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}graph_export.inl
)

add_subdirectories()
