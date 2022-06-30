#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_PROJECTS_FOLDER "neo shared")
set(CPP_GENERATE_TOOL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/source/generate_cpp_array.cpp
)
add_executable(cpp_generate_tool "${CPP_GENERATE_TOOL_SOURCES}")
set_target_properties(cpp_generate_tool PROPERTIES FOLDER "${SHARED_PROJECTS_FOLDER}")
