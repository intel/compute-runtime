#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(builtins_vme_sources)
set_target_properties(builtins_vme_sources PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_BUILTINS_PROJECTS_FOLDER}")
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} builtins_vme_sources)
add_subdirectories()
set(GENERATED_BUILTINS ${GENERATED_BUILTINS} PARENT_SCOPE)
set(GENERATED_BUILTINS_STATELESS ${GENERATED_BUILTINS_STATELESS} PARENT_SCOPE)

if("${NEO_ARCH}" STREQUAL "x32")
  set(BUILTIN_OPTIONS "-cl-intel-greater-than-4GB-buffer-required")
else()
  set(BUILTIN_OPTIONS "")
endif()

set(BUILTIN_OPTIONS_STATELESS
    "-cl-intel-greater-than-4GB-buffer-required"
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  list(APPEND __ocloc__options__ "-D DEBUG")
endif()

set(BUILTINS_INCLUDE_DIR ${TargetDir} PARENT_SCOPE)
set(BUILTIN_CPP "")

function(get_bits_for_stateless gen_type platform_type)
  # Force 32bits compiling on gen9lp for stateless builtins
  if((${GEN_TYPE} STREQUAL "GEN9") AND (${PLATFORM_TYPE} STREQUAL "LP"))
    set(BITS "32" PARENT_SCOPE)
  else()
    set(BITS ${NEO_BITS} PARENT_SCOPE)
  endif()
endfunction()

# Define function for compiling built-ins (with ocloc)
function(compile_builtin gen_type platform_type builtin bits builtin_options)
  string(TOLOWER ${gen_type} gen_type_lower)
  get_family_name_with_type(${gen_type} ${platform_type})
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${gen_type_lower}")
  # get filename
  set(FILENAME ${builtin})
  # get name of the file w/o extension
  get_filename_component(BASENAME ${builtin} NAME_WE)

  set(OUTPUTPATH_BASE "${OUTPUTDIR}/${BASENAME}_${family_name_with_type}")

  # function returns builtin cpp filename
  unset(BUILTIN_CPP)
  list(APPEND __ocloc__options__ "-cl-kernel-arg-info")
  if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
    set(OUTPUT_FILES
        ${OUTPUTPATH_BASE}.spv
        ${OUTPUTPATH_BASE}.bin
        ${OUTPUTPATH_BASE}.cpp
        ${OUTPUTPATH_BASE}.gen
    )

    add_custom_command(
                       OUTPUT ${OUTPUT_FILES}
                       COMMAND ${ocloc_cmd_prefix} -q -file ${FILENAME} -device ${DEFAULT_SUPPORTED_${gen_type}_${platform_type}_PLATFORM} ${builtin_options} -${bits} -out_dir ${OUTPUTDIR} -options "$<JOIN:${__ocloc__options__}, >"
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${builtin} ocloc copy_compiler_files
    )
    # set variable outside function
    set(BUILTIN_CPP built_ins/${NEO_ARCH}/${gen_type_lower}/${BASENAME}_${family_name_with_type}.cpp PARENT_SCOPE)
  else()
    foreach(_file_name "spv" "bin" "gen")
      set(_file_prebuilt "${NEO_SOURCE_DIR}/../neo_test_kernels/built_ins/${NEO_ARCH}/${gen_type_lower}/${BASENAME}_${family_name_with_type}.${_file_name}")
      if(EXISTS ${_file_prebuilt})
        add_custom_command(
                           OUTPUT ${OUTPUTPATH_BASE}.${_file_name}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
        )
      endif()
    endforeach()
    set(_file_prebuilt "${NEO_SOURCE_DIR}/../neo_test_kernels/built_ins/${NEO_ARCH}/${gen_type_lower}/${BASENAME}_${family_name_with_type}.cpp")
    if(EXISTS ${_file_prebuilt})
      add_custom_command(
                         OUTPUT ${OUTPUTPATH_BASE}.cpp
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
      )
      # set variable outside function
      set(BUILTIN_CPP built_ins/${NEO_ARCH}/${gen_type_lower}/${BASENAME}_${family_name_with_type}.cpp PARENT_SCOPE)
    endif()
  endif()
endfunction()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
      unset(BUILTINS_COMMANDS)
      foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
        compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}")
        if(NOT ${BUILTIN_CPP} STREQUAL "")
          list(APPEND BUILTINS_COMMANDS ${TargetDir}/${BUILTIN_CPP})
        endif()
      endforeach()
      get_bits_for_stateless(${GEN_TYPE} ${PLATFORM_TYPE})
      foreach(GENERATED_BUILTIN_STATELESS ${GENERATED_BUILTINS_STATELESS})
        compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN_STATELESS}.builtin_kernel ${BITS} "${BUILTIN_OPTIONS_STATELESS}")
        if(NOT ${BUILTIN_CPP} STREQUAL "")
          list(APPEND BUILTINS_COMMANDS ${TargetDir}/${BUILTIN_CPP})
        endif()
      endforeach()
      if(NOT "${BUILTINS_COMMANDS}" STREQUAL "")
        set(target_name builtins_${family_name_with_type}_vme)
        add_custom_target(${target_name} DEPENDS ${BUILTINS_COMMANDS})
        add_dependencies(builtins ${target_name})
        set_target_properties(${target_name} PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/built_ins/${family_name_with_type}")
      endif()
    endif()
  endforeach()
endmacro()

apply_macro_for_each_gen("SUPPORTED")
