#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_PROGRAM
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/build.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}create_ext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/link.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/process_device_binary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/process_intermediate_binary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/program.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_PROGRAM})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_PROGRAM ${RUNTIME_SRCS_PROGRAM})
