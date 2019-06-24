#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(${BUILTINS_SOURCES_LIB_NAME} OBJECT EXCLUDE_FROM_ALL
  CMakeLists.txt
  built_ins_registry.h
  register_copy_kernels_source.cpp
  register_ext_vme_source.cpp
)

set_target_properties(${BUILTINS_SOURCES_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_SOURCES_LIB_NAME} PROPERTIES FOLDER "built_ins")

target_include_directories(${BUILTINS_SOURCES_LIB_NAME} PRIVATE
  ${ENGINE_NODE_DIR}
  ${KHRONOS_HEADERS_DIR}
  ${KHRONOS_GL_HEADERS_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
)
