# Copyright (c) 2017 - 2018, Intel Corporation
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
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_LIB_NAME} builtins)

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
function(compile_builtin gen_type platform_type builtin)
  string(TOLOWER ${gen_type} gen_type_lower)
  get_family_name_with_type(${gen_type} ${platform_type})
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${gen_type_lower}")
  # get filename
  get_filename_component(FILENAME ${builtin} NAME)

  # get name of the file w/o extension
  get_filename_component(BASENAME ${builtin} NAME_WE)

  set(OUTPUTPATH_BASE "${OUTPUTDIR}/${BASENAME}_${family_name_with_type}")
  set(OUTPUT_FILES
    ${OUTPUTPATH_BASE}.bc
    ${OUTPUTPATH_BASE}.bin
    ${OUTPUTPATH_BASE}.cpp
    ${OUTPUTPATH_BASE}.gen
  )

  # function returns builtin cpp filename
  unset(BUILTIN_CPP)
  # set variable outside function
  set(BUILTIN_CPP built_ins/${NEO_ARCH}/${gen_type_lower}/${BASENAME}_${family_name_with_type}.cpp PARENT_SCOPE)
  if(WIN32)
    set(cloc_cmd_prefix cloc)
  else()
    if(DEFINED IGDRCL__IGC_LIBRARY_PATH)
      set(cloc_cmd_prefix LD_LIBRARY_PATH=${IGDRCL__IGC_LIBRARY_PATH} $<TARGET_FILE:cloc>)
    else()
      set(cloc_cmd_prefix LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc>)
    endif()
  endif()
  add_custom_command(
    OUTPUT ${OUTPUT_FILES}
    COMMAND ${cloc_cmd_prefix} -q -file ${FILENAME} -device ${DEFAULT_SUPPORTED_${gen_type}_${platform_type}_PLATFORM} ${BUILTIN_OPTIONS} -${NEO_BITS} -out_dir ${OUTPUTDIR}  -cpp_file -options "-cl-kernel-arg-info ${BUILTIN_DEBUG_OPTION}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${builtin} cloc copy_compiler_files
  )
endfunction()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE "CORE" "LP")
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
      unset(BUILTINS_COMMANDS)
      foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
        compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN}.igdrcl_built_in)
        list(APPEND BUILTINS_COMMANDS ${TargetDir}/${BUILTIN_CPP})
        set(RUNTIME_GENERATED_${GENERATED_BUILTIN}_${family_name_with_type} ${BUILTIN_CPP} PARENT_SCOPE)
      endforeach()

      set(target_name builtins_${family_name_with_type})
      add_custom_target(${target_name} DEPENDS ${BUILTINS_COMMANDS})
      add_dependencies(builtins ${target_name})
      set_target_properties(${target_name} PROPERTIES FOLDER "built_ins/${family_name_with_type}")
    endif()
  endforeach()
endmacro()

apply_macro_for_each_gen("SUPPORTED")
