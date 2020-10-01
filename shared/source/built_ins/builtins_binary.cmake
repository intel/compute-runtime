#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} OBJECT EXCLUDE_FROM_ALL builtins_binary.cmake)
add_library(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} OBJECT EXCLUDE_FROM_ALL builtins_binary.cmake)

# Add builtins sources
add_subdirectory(registry)

list(APPEND BIND_MODES
     "bindful"
     #            "bindless"
)

set(GENERATED_BUILTINS
    "aux_translation"
    "copy_buffer_rect"
    "copy_buffer_to_buffer"
    "copy_buffer_to_image3d"
    "copy_kernel_timestamps"
    "fill_buffer"
    "fill_image1d"
    "fill_image2d"
    "fill_image3d"
)

set(GENERATED_BUILTINS_IMAGES
    "copy_image3d_to_buffer"
    "copy_image_to_image1d"
    "copy_image_to_image2d"
    "copy_image_to_image3d"
)

set(GENERATED_BUILTINS_IMAGES_STATELESS
    "copy_image3d_to_buffer_stateless"
)

set(GENERATED_BUILTINS_STATELESS
    "copy_buffer_to_buffer_stateless"
    "copy_buffer_to_image3d_stateless"
    "copy_buffer_rect_stateless"
    "fill_buffer_stateless"
)

# Generate builtins cpps
if(COMPILE_BUILT_INS)
  add_subdirectory(kernels)
endif()

macro(macro_for_each_gen)
  foreach(MODE ${BIND_MODES})
    foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      foreach(GENERATED_BUILTIN_IMAGES ${GENERATED_BUILTINS_IMAGES})
        list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN_IMAGES}_${family_name_with_type}_${MODE}})
      endforeach()
      foreach(GENERATED_BUILTIN_IMAGES_STATELESS ${GENERATED_BUILTINS_IMAGES_STATELESS})
        list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN_IMAGES_STATELESS}_${family_name_with_type}_${MODE}})
      endforeach()
      foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
        list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN}_${family_name_with_type}_${MODE}})
      endforeach()
      foreach(GENERATED_BUILTIN_STATELESS ${GENERATED_BUILTINS_STATELESS})
        list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN_STATELESS}_${family_name_with_type}_${MODE}})
      endforeach()
    endforeach()
    source_group("generated files\\${GEN_TYPE_LOWER}" FILES ${GENERATED_BUILTINS_CPPS_${MODE}})
  endforeach()
endmacro()

apply_macro_for_each_gen("SUPPORTED")

if(COMPILE_BUILT_INS)
  target_sources(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} PUBLIC ${GENERATED_BUILTINS_CPPS_bindful})
  set_source_files_properties(${GENERATED_BUILTINS_CPPS_bindful} PROPERTIES GENERATED TRUE)
endif()

set_target_properties(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} PROPERTIES FOLDER "built_ins")

target_include_directories(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
                           ${THIRD_PARTY_DIR}
)

if(COMPILE_BUILT_INS)
  target_sources(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} PUBLIC ${GENERATED_BUILTINS_CPPS_bindless})
  set_source_files_properties(${GENERATED_BUILTINS_CPPS_bindless} PROPERTIES GENERATED TRUE)
endif()

set_target_properties(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} PROPERTIES FOLDER "built_ins")

target_include_directories(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
                           ${THIRD_PARTY_DIR}
)
