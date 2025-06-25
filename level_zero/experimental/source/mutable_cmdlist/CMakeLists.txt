#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/label.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/label.h
               ${CMAKE_CURRENT_SOURCE_DIR}/label_handle.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_imp_experimental.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_hw_experimental.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/variable_experimental.cpp
)

add_subdirectories()
