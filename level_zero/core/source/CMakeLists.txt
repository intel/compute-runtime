#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/get_extension_function_lookup_map.h
               ${CMAKE_CURRENT_SOURCE_DIR}/global_teardown.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/global_teardown.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}get_extension_function_lookup_map.cpp
)

add_subdirectories()

