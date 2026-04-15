#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(BUILTINS_BUFFER
    "copy_buffer_rect"
    "copy_buffer_to_buffer"
    "copy_kernel_timestamps"
    "fill_buffer"
)

set(BUILTINS_AUX_TRANSLATION
    "aux_translation"
)

set(BUILTINS_IMAGE_BUFFER
    "copy_buffer_to_image3d"
    "copy_image3d_to_buffer"
)

set(BUILTINS_IMAGE
    "copy_image_to_image1d"
    "copy_image_to_image2d"
    "copy_image_to_image3d"
    "fill_image1d"
    "fill_image2d"
    "fill_image3d"
    "fill_image1d_buffer"
)

# Heapless platforms always compile with -heapless_mode enable
set(HEAPLESS_CORE_TYPES "XE3P_CORE")

add_library(${BUILTINS_BINARIES_LIB_NAME} OBJECT EXCLUDE_FROM_ALL builtins_binary.cmake)
target_compile_definitions(${BUILTINS_BINARIES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)

# Add builtins sources
add_subdirectory(registry)

# Generate builtins cpps
if(COMPILE_BUILT_INS)
  add_subdirectory(kernels)
endif()

get_property(GENERATED_BUILTINS_CPPS GLOBAL PROPERTY GENERATED_BUILTINS_CPPS)

if(COMPILE_BUILT_INS)
  target_sources(${BUILTINS_BINARIES_LIB_NAME} PUBLIC ${GENERATED_BUILTINS_CPPS})
  set_source_files_properties(${GENERATED_BUILTINS_CPPS} PROPERTIES GENERATED TRUE)
endif()

set_target_properties(${BUILTINS_BINARIES_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${BUILTINS_BINARIES_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_BINARIES_LIB_NAME} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")

target_include_directories(${BUILTINS_BINARIES_LIB_NAME} PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
)
