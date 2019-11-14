#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(${BUILTINS_BINARIES_LIB_NAME} OBJECT EXCLUDE_FROM_ALL builtins_binary.cmake)

# Add builtins sources
add_subdirectory(registry)

set(GENERATED_BUILTINS
  "aux_translation"
  "copy_buffer_rect"
  "copy_buffer_to_buffer"
  "copy_buffer_to_image3d"
  "copy_image3d_to_buffer"
  "copy_image_to_image1d"
  "copy_image_to_image2d"
  "copy_image_to_image3d"
  "fill_buffer"
  "fill_image1d"
  "fill_image2d"
  "fill_image3d"
)

set(GENERATED_BUILTINS_STATELESS
  "copy_buffer_to_buffer_stateless"
  "copy_buffer_rect_stateless"
  "copy_buffer_to_image3d_stateless"
  "fill_buffer_stateless"
)

# Generate builtins cpps
if(COMPILE_BUILT_INS)
  add_subdirectory(kernels)
endif()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
    foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
      list(APPEND GENERATED_BUILTINS_CPPS ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN}_${family_name_with_type}})
    endforeach()
    foreach(GENERATED_BUILTIN_STATELESS ${GENERATED_BUILTINS_STATELESS})
      list(APPEND GENERATED_BUILTINS_CPPS ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN_STATELESS}_${family_name_with_type}})
    endforeach()
  endforeach()
  source_group("generated files\\${GEN_TYPE_LOWER}" FILES ${GENERATED_BUILTINS_CPPS})
endmacro()

apply_macro_for_each_gen("SUPPORTED")

if(COMPILE_BUILT_INS)
  target_sources(${BUILTINS_BINARIES_LIB_NAME} PUBLIC ${GENERATED_BUILTINS_CPPS})
  set_source_files_properties(${GENERATED_BUILTINS_CPPS} PROPERTIES GENERATED TRUE)
endif()

set_target_properties(${BUILTINS_BINARIES_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${BUILTINS_BINARIES_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_BINARIES_LIB_NAME} PROPERTIES FOLDER "built_ins")

target_include_directories(${BUILTINS_BINARIES_LIB_NAME} PRIVATE
  ${ENGINE_NODE_DIR}
  ${KHRONOS_HEADERS_DIR}
  ${KHRONOS_GL_HEADERS_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
)
