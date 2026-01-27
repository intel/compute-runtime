#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

list(APPEND HEAPLESS_ADDRESSING_MODES
     "stateless_heapless"
     "wide_stateless_heapless"
)

list(APPEND STATELESS_ADDRESSING_MODES
     "stateless"
     "wide_stateless"
     ${HEAPLESS_ADDRESSING_MODES}
)

list(APPEND ADDRESSING_MODES
     "bindful"
     "bindless"
     ${STATELESS_ADDRESSING_MODES}
)

set(GENERATED_BUILTINS
    "copy_buffer_rect"
    "copy_buffer_to_buffer"
    "copy_kernel_timestamps"
    "fill_buffer"
)

set(GENERATED_BUILTINS_AUX_TRANSLATION
    "aux_translation"
)

set(GENERATED_BUILTINS_IMAGES
    "copy_buffer_to_image3d"
    "copy_image3d_to_buffer"
    "copy_image_to_image1d"
    "copy_image_to_image2d"
    "copy_image_to_image3d"
    "fill_image1d"
    "fill_image2d"
    "fill_image3d"
    "fill_image1d_buffer"
)

set(GENERATED_BUILTINS_IMAGES_STATELESS
    "copy_buffer_to_image3d"
    "copy_image3d_to_buffer"
)

set(GENERATED_BUILTINS_STATELESS
    "copy_buffer_to_buffer"
    "copy_buffer_rect"
    "fill_buffer"
)

foreach(MODE ${ADDRESSING_MODES})
  string(TOUPPER ${MODE} MODE_UPPER_CASE)
  add_library(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} OBJECT EXCLUDE_FROM_ALL builtins_binary.cmake)
  target_compile_definitions(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
endforeach()

# Add builtins sources
add_subdirectory(registry)

# Generate builtins cpps
if(COMPILE_BUILT_INS)
  add_subdirectory(kernels)
endif()

foreach(MODE ${ADDRESSING_MODES})
  string(TOUPPER ${MODE} MODE_UPPER_CASE)

  get_property(GENERATED_BUILTINS_CPPS_${MODE} GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE})
  source_group("generated files\\${CORE_TYPE_LOWER}" FILES GENERATED_BUILTINS_CPPS_${MODE})

  if(COMPILE_BUILT_INS)
    target_sources(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} PUBLIC ${GENERATED_BUILTINS_CPPS_${MODE}})
    set_source_files_properties(${GENERATED_BUILTINS_CPPS_${MODE}} PROPERTIES GENERATED TRUE)
  endif()

  set_target_properties(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
  set_target_properties(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")

  target_include_directories(${BUILTINS_BINARIES_${MODE_UPPER_CASE}_LIB_NAME} PRIVATE
                             ${ENGINE_NODE_DIR}
                             ${KHRONOS_HEADERS_DIR}
                             ${KHRONOS_GL_HEADERS_DIR}
                             ${NEO__GMM_INCLUDE_DIR}
                             ${NEO__IGC_INCLUDE_DIR}
  )
endforeach()
