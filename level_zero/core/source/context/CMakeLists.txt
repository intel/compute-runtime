#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}context_imp_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context_imp_${DRIVER_MODEL}/context_imp_${DRIVER_MODEL}.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context_imp_${DRIVER_MODEL}${BRANCH_DIR_SUFFIX}context_imp_${DRIVER_MODEL}_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/context_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/context.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/context_ext.cpp
)

add_subdirectories()
