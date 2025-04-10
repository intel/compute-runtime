#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_BUILTINS_PROJECTS_FOLDER "built_ins")
set(NEO_CORE_SRCS_BUILT_INS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.h
    ${CMAKE_CURRENT_SOURCE_DIR}/built_in_ops_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/registry/built_ins_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sip_kernel_type.h
)

add_subdirectory(builtinops)

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_BUILT_INS ${NEO_CORE_SRCS_BUILT_INS})

set(NEO_CORE_SRCS_BUILT_IN_KERNELS
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/aux_translation.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_rect.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_rect_stateless.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_buffer.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_buffer_stateless.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_image3d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_image3d_stateless.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_kernel_timestamps.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image3d_to_buffer.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image3d_to_buffer_stateless.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image1d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image2d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image3d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_buffer.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_buffer_stateless.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image1d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image2d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image3d.builtin_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image1d_buffer.builtin_kernel
)

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_BUILT_IN_KERNELS ${NEO_CORE_SRCS_BUILT_IN_KERNELS})

if(NOT (TARGET ${BUILTINS_BINARIES_BINDFUL_LIB_NAME}))
  include(builtins_binary.cmake)
endif()
