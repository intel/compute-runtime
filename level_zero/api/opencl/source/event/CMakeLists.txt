#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_EVENT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/regular_events_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/regular_events_manager.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_EVENT_SRCS})
set_property(GLOBAL PROPERTY LEO_EVENT_SRCS ${LEO_EVENT_SRCS})
