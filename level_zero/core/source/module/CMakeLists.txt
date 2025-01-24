#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/module.h
               ${CMAKE_CURRENT_SOURCE_DIR}/module_build_log.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/module_build_log.h
               ${CMAKE_CURRENT_SOURCE_DIR}/module_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/module_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/translation_unit_ext.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}module_imp_helper.cpp
)

add_subdirectories()
