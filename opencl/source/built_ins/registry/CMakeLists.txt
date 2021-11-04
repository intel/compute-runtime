#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(${BUILTINS_VME_LIB_NAME} OBJECT EXCLUDE_FROM_ALL
            CMakeLists.txt
            register_ext_vme_source.cpp
)
set_target_properties(${BUILTINS_VME_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_VME_LIB_NAME} PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_BUILTINS_PROJECTS_FOLDER}")

target_include_directories(${BUILTINS_VME_LIB_NAME} PRIVATE
                           ${KHRONOS_HEADERS_DIR}
)
