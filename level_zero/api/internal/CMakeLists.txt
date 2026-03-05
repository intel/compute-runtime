#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_cmdlist.h
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_context.h
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_driver.h
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_graph.h
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_internal.h
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_module.h
)

add_subdirectories()
