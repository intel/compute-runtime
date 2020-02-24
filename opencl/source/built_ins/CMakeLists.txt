#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_BUILT_INS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aux_translation_builtin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/builtins_dispatch_builder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/builtins_dispatch_builder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_in_ops_vme.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.inl
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/unknown_built_in_name.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vme_builtin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vme_builtin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/vme_dispatch_builder.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_INS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_BUILT_INS ${RUNTIME_SRCS_BUILT_INS})

set(RUNTIME_SRCS_BUILT_IN_KERNELS
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.builtin_kernel
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_advanced_motion_estimate_check_intel.builtin_kernel
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_motion_estimate_intel.builtin_kernel
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_IN_KERNELS})

if(NOT (TARGET ${BUILTINS_VME_LIB_NAME}))
  add_subdirectory(registry)
  if(COMPILE_BUILT_INS)
    add_subdirectory(kernels)
  endif()
endif()
