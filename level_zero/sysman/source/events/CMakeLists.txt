#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_events.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_events_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_events_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_events.h
)

add_subdirectories()

