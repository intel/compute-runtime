#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_TRACING_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_tracing_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_tracing_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_tracing_handle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_tracing_notify.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_tracing_types.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_TRACING_SRCS})
set_property(GLOBAL PROPERTY LEO_TRACING_SRCS ${LEO_TRACING_SRCS})
add_subdirectories()
