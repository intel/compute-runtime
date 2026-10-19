#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)
project(align2m_preload CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "align2m_preload is supported on Linux only")
endif()

add_library(align2m_preload SHARED ${CMAKE_CURRENT_SOURCE_DIR}/source/align2m_preload.cpp)
set_target_properties(align2m_preload PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(align2m_preload PRIVATE _GNU_SOURCE)
target_compile_options(align2m_preload PRIVATE
                       -fno-builtin-malloc
                       -fno-builtin-calloc
                       -fno-builtin-realloc
                       -fno-builtin-free
)
target_link_libraries(align2m_preload PRIVATE dl pthread)
