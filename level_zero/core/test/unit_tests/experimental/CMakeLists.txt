#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph_export.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_graph_export.cpp
)

target_include_directories(${TARGET_NAME}
                           PUBLIC
                           "${NEO_SOURCE_DIR}/level_zero/experimental/source/graph/captured_apis${BRANCH_DIR_SUFFIX}"
)

add_subdirectories()
