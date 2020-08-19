#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CPP_GENERATE_TOOL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/source/generate_cpp_array.cpp
)
add_executable(cpp_generate_tool "${CPP_GENERATE_TOOL_SOURCES}")
