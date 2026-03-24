#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}context_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context_${DRIVER_MODEL}/context_${DRIVER_MODEL}.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context_${DRIVER_MODEL}${BRANCH_DIR_SUFFIX}context_${DRIVER_MODEL}_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/context_ext.cpp
)

add_subdirectories()
