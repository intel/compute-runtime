#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(${BUILTINS_SOURCES_LIB_NAME} OBJECT EXCLUDE_FROM_ALL
            CMakeLists.txt
            built_ins_registry.h
            register_copy_kernels_source.cpp
)

set_target_properties(${BUILTINS_SOURCES_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_SOURCES_LIB_NAME} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")

target_include_directories(${BUILTINS_SOURCES_LIB_NAME} PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
                           ${THIRD_PARTY_DIR}
)

add_subdirectories()
