#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(mock_aubstream)

add_library(
            mock_aubstream EXCLUDE_FROM_ALL OBJECT
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/mock_aubstream.cpp
)

create_project_source_tree(mock_aubstream)
set_target_properties(mock_aubstream PROPERTIES FOLDER "test mocks")
target_include_directories(mock_aubstream PRIVATE ${AUB_STREAM_HEADERS_DIR})
