#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(BUILTINS_KERNELS_DIR "${NEO_SOURCE_DIR}/shared/source/built_ins/kernels")

file(READ "${BUILTINS_KERNELS_DIR}/copy_buffer_to_buffer.builtin_kernel" COPY_BUFFER_TO_BUFFER_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_buffer_rect.builtin_kernel" COPY_BUFFER_RECT_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/fill_buffer.builtin_kernel" FILL_BUFFER_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_buffer_to_image3d.builtin_kernel" COPY_BUFFER_TO_IMAGE3D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_image3d_to_buffer.builtin_kernel" COPY_IMAGE3D_TO_BUFFER_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_image_to_image1d.builtin_kernel" COPY_IMAGE_TO_IMAGE1D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_image_to_image2d.builtin_kernel" COPY_IMAGE_TO_IMAGE2D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_image_to_image3d.builtin_kernel" COPY_IMAGE_TO_IMAGE3D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/fill_image1d.builtin_kernel" FILL_IMAGE1D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/fill_image2d.builtin_kernel" FILL_IMAGE2D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/fill_image3d.builtin_kernel" FILL_IMAGE3D_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/aux_translation.builtin_kernel" AUX_TRANSLATION_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/copy_kernel_timestamps.builtin_kernel" COPY_KERNEL_TIMESTAMPS_SRC)
file(READ "${BUILTINS_KERNELS_DIR}/fill_image1d_buffer.builtin_kernel" FILL_IMAGE1D_BUFFER_SRC)

configure_file(
               "${CMAKE_CURRENT_SOURCE_DIR}/register_copy_kernels_source.cpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/register_copy_kernels_source.cpp"
               @ONLY
)

add_library(${BUILTINS_SOURCES_LIB_NAME} OBJECT EXCLUDE_FROM_ALL
            CMakeLists.txt
            built_ins_registry.h
            "${CMAKE_CURRENT_BINARY_DIR}/register_copy_kernels_source.cpp"
)

set_target_properties(${BUILTINS_SOURCES_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_SOURCES_LIB_NAME} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")

target_include_directories(${BUILTINS_SOURCES_LIB_NAME} PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
)

add_subdirectories()
