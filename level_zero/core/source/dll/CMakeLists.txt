#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/disallow_deferred_deleter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_builtin_functions_lib.cpp
)
add_subdirectories()
if(WIN32)
  append_sources_from_properties(L0_SRCS_DLL L0_SRCS_DLL_WINDOWS)
else()
  append_sources_from_properties(L0_SRCS_DLL L0_SRCS_DLL_LINUX)
endif()
set_property(GLOBAL PROPERTY L0_SRCS_DLL ${L0_SRCS_DLL})
