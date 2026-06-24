#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_GTPIN
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_notify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_notify.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GTPIN})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GTPIN ${RUNTIME_SRCS_GTPIN})

add_subdirectories()
