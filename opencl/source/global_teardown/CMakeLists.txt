#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_GLOBAL_TEARDOWN
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/global_platform_teardown.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/global_platform_teardown.h
)
add_subdirectories()
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GLOBAL_TEARDOWN})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GLOBAL_TEARDOWN ${RUNTIME_SRCS_GLOBAL_TEARDOWN})
