#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_cmdlist.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_context.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_driver.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_event.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_memory.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_module.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_metric.cpp
)

add_subdirectories()
