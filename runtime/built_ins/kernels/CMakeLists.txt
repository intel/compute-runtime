# Copyright (c) 2017, Intel Corporation
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

add_custom_target(builtins)
set_target_properties(builtins PROPERTIES FOLDER "built_ins")
set (BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_LIB_NAME} builtins)

# Set variables needed to compile built-in kernels
set (COPY_BUFFER_TO_BUFFER_BUILTIN copy_buffer_to_buffer.igdrcl_built_in)
set (COPY_BUFFER_RECT_BUILTIN copy_buffer_rect.igdrcl_built_in)
set (FILL_BUFFER_BUILTIN fill_buffer.igdrcl_built_in)
set (COPY_BUFFER_TO_IMAGE3D_BUILTIN copy_buffer_to_image3d.igdrcl_built_in)
set (COPY_IMAGE3D_TO_BUFFER_BUILTIN copy_image3d_to_buffer.igdrcl_built_in)
set (COPY_IMAGE_TO_IMAGE1D_BUILTIN copy_image_to_image1d.igdrcl_built_in)
set (COPY_IMAGE_TO_IMAGE2D_BUILTIN copy_image_to_image2d.igdrcl_built_in)
set (COPY_IMAGE_TO_IMAGE3D_BUILTIN copy_image_to_image3d.igdrcl_built_in)
set (FILL_IMAGE1D_BUILTIN fill_image1d.igdrcl_built_in)
set (FILL_IMAGE2D_BUILTIN fill_image2d.igdrcl_built_in)
set (FILL_IMAGE3D_BUILTIN fill_image3d.igdrcl_built_in)

if("${NEO_ARCH}" STREQUAL "x32")
    set(BUILTIN_OPTIONS "-cl-intel-greater-than-4GB-buffer-required")
else()
    set(BUILTIN_OPTIONS "")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
    set(BUILTIN_DEBUG_OPTION "-D DEBUG")
else()
    set(BUILTIN_DEBUG_OPTION "")
endif()

set(BUILTINS_INCLUDE_DIR ${TargetDir} PARENT_SCOPE)
set(BUILTIN_CPP "")

# Define function for compiling built-ins (with cloc)
function(compile_builtin gen_name builtin)
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${gen_name}")

  # get filename
  get_filename_component(FILENAME ${builtin} NAME)

  # get name of the file w/o extension
  get_filename_component(BASENAME ${builtin} NAME_WE)

  set(OUTPUTPATH_BASE "${OUTPUTDIR}/${BASENAME}_${gen_name}")
  set(OUTPUT_FILES
    ${OUTPUTPATH_BASE}.bc
    ${OUTPUTPATH_BASE}.bin
    ${OUTPUTPATH_BASE}.cpp
    ${OUTPUTPATH_BASE}.gen
  )

  # function returns builtin cpp filename
  unset(BUILTIN_CPP)
  # set variable outside function
  set(BUILTIN_CPP built_ins/${NEO_ARCH}/${gen_name}/${BASENAME}_${gen_name}.cpp PARENT_SCOPE)

  if(MSVC)
    add_custom_command(
      OUTPUT ${OUTPUT_FILES}
      COMMAND cloc -q -file ${FILENAME} -device ${gen_name} ${BUILTIN_OPTIONS} -${NEO_BITS} -out_dir ${OUTPUTDIR} -cpp_file -options "-cl-kernel-arg-info ${BUILTIN_DEBUG_OPTION}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${builtin} cloc copy_compiler_files
    )
  else()
    add_custom_command(
      OUTPUT ${OUTPUT_FILES}
      COMMAND LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc> -q -file ${FILENAME} -device ${gen_name} ${BUILTIN_OPTIONS} -${NEO_BITS} -out_dir ${OUTPUTDIR} -cpp_file -options "-cl-kernel-arg-info ${BUILTIN_DEBUG_OPTION}"
      WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${builtin} cloc copy_compiler_files
    )
  endif()
endfunction()

macro(compile_builtins GEN_NUM PLATFORM_IT)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_LOWER)
  string(CONCAT GEN "_GEN" ${GEN_NUM} "_" ${PLATFORM_IT})

  set (BUILTINS_COMMANDS)
  foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
    compile_builtin(${PLATFORM_LOWER} ${${GENERATED_BUILTIN}_BUILTIN})
    list(APPEND BUILTINS_COMMANDS ${TargetDir}/${BUILTIN_CPP})
    set (RUNTIME_GENERATED_${GENERATED_BUILTIN}${GEN} ${BUILTIN_CPP} PARENT_SCOPE)
  endforeach(GENERATED_BUILTIN)

  set(target_name builtins_${PLATFORM_LOWER})
  add_custom_target(${target_name} DEPENDS ${BUILTINS_COMMANDS})
  add_dependencies(builtins ${target_name})
  set_target_properties(${target_name} PROPERTIES FOLDER "built_ins/${PLATFORM_LOWER}")
endmacro()

# Compile built-in kernels for all GENs
foreach(GEN_NUM RANGE ${MAX_GEN} 0 -1)
  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    GET_PLATFORMS_FOR_GEN("SUPPORTED" ${GEN_NUM} SUPPORTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${SUPPORTED_GENX_PLATFORMS})
      compile_builtins(${GEN_NUM} ${PLATFORM_IT})
    endforeach()
  endif()
endforeach()
