# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

add_library(${BUILTINS_BINARIES_LIB_NAME} OBJECT builtins_binary.cmake)

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

# Generate builtins cpps
if(COMPILE_BUILT_INS)
  add_subdirectory(kernels)
endif()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE "CORE" "LP")
    get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
    foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
      list(APPEND GENERATED_BUILTINS_CPPS ${BUILTINS_INCLUDE_DIR}/${RUNTIME_GENERATED_${GENERATED_BUILTIN}_${family_name_with_type}})
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
  ${KHRONOS_HEADERS_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
)
