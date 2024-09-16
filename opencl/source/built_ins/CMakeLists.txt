#
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_BUILT_INS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aux_translation_builtin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/builtins_dispatch_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/builtins_dispatch_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.inl
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_INS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_BUILT_INS ${RUNTIME_SRCS_BUILT_INS})

add_subdirectories()

