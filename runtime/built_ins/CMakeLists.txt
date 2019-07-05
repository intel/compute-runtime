#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_BUILT_INS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aux_translation_builtin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/builtins_dispatch_builder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_storage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_in_ops_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/builtinops${BRANCH_DIR_SUFFIX}/built_in_ops.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/sip.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sip.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/unknown_built_in_name.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vme_dispatch_builder.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_INS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_BUILT_INS ${RUNTIME_SRCS_BUILT_INS})

set(RUNTIME_SRCS_BUILT_IN_KERNELS
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/aux_translation.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_rect.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_buffer.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_image3d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image3d_to_buffer.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image1d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image2d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image3d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_buffer.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image1d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image2d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image3d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_motion_estimate_intel.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_advanced_motion_estimate_check_intel.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.igdrcl_built_in
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_IN_KERNELS})

if(NOT (TARGET ${BUILTINS_BINARIES_LIB_NAME}))
  include(builtins_binary.cmake)
endif()
