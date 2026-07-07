#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_MEM_OBJ_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_map_operations_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_map_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_mem_obj.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_mem_obj.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_mem_obj_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_mem_obj_helper.h
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_MEM_OBJ_SRCS})
set_property(GLOBAL PROPERTY LEO_MEM_OBJ_SRCS ${LEO_MEM_OBJ_SRCS})
add_subdirectories()
