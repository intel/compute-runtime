#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_exp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph_export.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph_export.cpp
)

add_subdirectories()
