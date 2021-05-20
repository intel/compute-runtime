#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(builtins)
set_target_properties(builtins PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUIILINS_PROJECTS_FOLDER}")
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} builtins)
add_subdirectories()
set(GENERATED_BUILTINS ${GENERATED_BUILTINS} PARENT_SCOPE)
set(GENERATED_BUILTINS_IMAGES ${GENERATED_BUILTINS_IMAGES} PARENT_SCOPE)
set(GENERATED_BUILTINS_STATELESS ${GENERATED_BUILTINS_STATELESS} PARENT_SCOPE)

if("${NEO_ARCH}" STREQUAL "x32")
  set(BUILTIN_OPTIONS "-cl-intel-greater-than-4GB-buffer-required")
else()
  set(BUILTIN_OPTIONS "")
endif()

set(BUILTIN_OPTIONS_STATELESS
    "-cl-intel-greater-than-4GB-buffer-required"
)

set(bindless_OPTIONS
    -internal_options "-cl-intel-use-bindless-mode"
)

set(bindful_OPTIONS
    ""
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  list(APPEND __cloc__options__ "-D DEBUG")
endif()

set(BUILTINS_INCLUDE_DIR ${TargetDir} PARENT_SCOPE)
set(BUILTIN_CPP "")

function(get_bits_for_stateless gen_type platform_type)
  # Force 32bits compiling on gen9lp for stateless builtins
  if((${gen_type} STREQUAL "GEN9") AND (${platform_type} STREQUAL "LP"))
    set(BITS "32" PARENT_SCOPE)
  else()
    set(BITS ${NEO_BITS} PARENT_SCOPE)
  endif()
endfunction()

# Define function for compiling built-ins (with ocloc)
function(compile_builtin gen_type platform_type builtin bits builtin_options mode)
  string(TOLOWER ${gen_type} gen_type_lower)
  get_family_name_with_type(${gen_type} ${platform_type})
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${gen_type_lower}")
  # get filename
  set(FILENAME ${builtin})
  # get name of the file w/o extension
  get_filename_component(BASENAME ${builtin} NAME_WE)

  set(OUTPUT_FILE_SPV
      ${OUTPUTDIR}/${mode}_${BASENAME}_${family_name_with_type}.spv
  )

  if(NOT DEFINED cloc_cmd_prefix)
    if(WIN32)
      set(cloc_cmd_prefix ocloc)
    else()
      if(DEFINED NEO__IGC_LIBRARY_PATH)
        set(cloc_cmd_prefix LD_LIBRARY_PATH=${NEO__IGC_LIBRARY_PATH}:$<TARGET_FILE_DIR:ocloc_lib> $<TARGET_FILE:ocloc>)
      else()
        set(cloc_cmd_prefix LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ocloc_lib> $<TARGET_FILE:ocloc>)
      endif()
    endif()
  endif()
  list(APPEND __cloc__options__ "-cl-kernel-arg-info")
  set(INTERNAL_OPTIONS "${${mode}_OPTIONS}")
  add_custom_command(
                     OUTPUT ${OUTPUT_FILE_SPV}
                     COMMAND ${cloc_cmd_prefix} -q -file ${FILENAME} -spv_only -device ${DEFAULT_SUPPORTED_${gen_type}_${platform_type}_PLATFORM} ${builtin_options} -${bits} -output ${mode}_${BASENAME} -out_dir ${OUTPUTDIR} ${INTERNAL_OPTIONS} -options "$<JOIN:${__cloc__options__}, >"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${builtin} ocloc copy_compiler_files
  )
  foreach(REVISION_ID ${${platform_type}_${gen_type}_REVISIONS})
    set(OUTPUT_FILE_CPP
        ${OUTPUTDIR}/${mode}_${BASENAME}_${family_name_with_type}_${REVISION_ID}.cpp
    )
    set(BINARY_OUTPUT "${OUTPUTDIR}/${mode}_${BASENAME}_${REVISION_ID}_${family_name_with_type}")
    set(OUTPUT_FILES_BINARIES
        ${BINARY_OUTPUT}.gen
        ${BINARY_OUTPUT}.bin
    )
    list(APPEND BUILTINS_COMMANDS "${OUTPUT_FILE_CPP}")
    add_custom_command(
                       OUTPUT ${OUTPUT_FILES_BINARIES}
                       COMMAND ${cloc_cmd_prefix} -q -file ${OUTPUT_FILE_SPV} -spirv_input -device ${DEFAULT_SUPPORTED_${gen_type}_${platform_type}_PLATFORM} ${builtin_options} -${bits} -output ${mode}_${BASENAME}_${REVISION_ID} -out_dir ${OUTPUTDIR} -revision_id ${REVISION_ID} ${INTERNAL_OPTIONS} -options "$<JOIN:${__cloc__options__}, >"
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${OUTPUT_FILE_SPV} ocloc copy_compiler_files
    )
    add_custom_command(
                       OUTPUT ${OUTPUT_FILE_CPP}
                       COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${BINARY_OUTPUT}.gen --output ${OUTPUT_FILE_CPP} --array ${mode}_${BASENAME} --platform ${family_name_with_type} --revision_id ${REVISION_ID}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${OUTPUT_FILES_BINARIES} $<TARGET_FILE:cpp_generate_tool>
    )
  endforeach()
  set(BUILTINS_COMMANDS ${BUILTINS_COMMANDS} PARENT_SCOPE)
endfunction()

function(generate_cpp_spirv builtin)
  if(NOT DEFINED cloc_cmd_prefix)
    if(WIN32)
      set(cloc_cmd_prefix $<TARGET_FILE:ocloc>)
    else()
      if(DEFINED NEO__IGC_LIBRARY_PATH)
        set(cloc_cmd_prefix LD_LIBRARY_PATH=${NEO__IGC_LIBRARY_PATH}:$<TARGET_FILE_DIR:ocloc_lib> $<TARGET_FILE:ocloc>)
      else()
        set(cloc_cmd_prefix LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ocloc_lib> $<TARGET_FILE:ocloc>)
      endif()
    endif()
  endif()

  get_filename_component(BASENAME ${builtin} NAME_WE)
  get_filename_component(DIR ${builtin} DIRECTORY)

  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/spirv/${DIR}")
  string(REPLACE "//" "/" OUTPUTDIR ${OUTPUTDIR})

  set(INPUT_FILENAME ${builtin}.builtin_kernel)

  set(GENERATED_SPV_INPUT ${OUTPUTDIR}/${BASENAME}.spv)
  set(OUTPUT_FILE_CPP
      ${OUTPUTDIR}/${BASENAME}.cpp
  )

  set(OUTPUT_LIST_SPV_FILES ${OUTPUT_LIST_SPV_FILES} ${OUTPUT_FILES_FOR_SPV} PARENT_SCOPE)
  set(OUTPUT_LIST_CPP_FILES ${OUTPUT_LIST_CPP_FILES} ${OUTPUT_FILE_CPP} PARENT_SCOPE)
  add_custom_command(
                     OUTPUT ${GENERATED_SPV_INPUT}
                     COMMAND ${cloc_cmd_prefix} -q -spv_only -file ${INPUT_FILENAME} -out_dir ${OUTPUTDIR} -output_no_suffix -options "-cl-kernel-arg-info"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${INPUT_FILENAME} ocloc copy_compiler_files
  )
  add_custom_command(
                     OUTPUT ${OUTPUT_FILE_CPP}
                     COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${GENERATED_SPV_INPUT} --output ${OUTPUT_FILE_CPP} --array ${BASENAME}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${GENERATED_SPV_INPUT} $<TARGET_FILE:cpp_generate_tool>
  )
endfunction()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})
      unset(IMAGE_SUPPORT)
      GEN_CONTAINS_PLATFORMS("SUPPORTED_IMAGES" ${GEN_TYPE} IMAGE_SUPPORT)
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
      get_bits_for_stateless(${GEN_TYPE} ${PLATFORM_TYPE})
      set(target_name builtins_${family_name_with_type})
      add_custom_target(${target_name})
      add_dependencies(builtins ${target_name})
      set_target_properties(${target_name} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUIILINS_PROJECTS_FOLDER}/${family_name_with_type}")
      foreach(MODE ${BIND_MODES})
        unset(BUILTINS_COMMANDS)
        foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
          compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
        endforeach()
        foreach(GENERATED_BUILTIN_STATELESS ${GENERATED_BUILTINS_STATELESS})
          compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN_STATELESS}.builtin_kernel ${BITS} "${BUILTIN_OPTIONS_STATELESS}" ${MODE})
        endforeach()
        if(${IMAGE_SUPPORT})
          foreach(GENERATED_BUILTINS_IMAGES ${GENERATED_BUILTINS_IMAGES})
            compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTINS_IMAGES}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
          endforeach()
          foreach(GENERATED_BUILTIN_IMAGES_STATELESS ${GENERATED_BUILTINS_IMAGES_STATELESS})
            compile_builtin(${GEN_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN_IMAGES_STATELESS}.builtin_kernel ${BITS} "${BUILTIN_OPTIONS_STATELESS}" ${MODE})
          endforeach()
        endif()
        get_property(GENERATED_BUILTINS_CPPS_${MODE} GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE})
        foreach(BUILTIN ${BUILTINS_COMMANDS})
          list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTIN})
        endforeach()
        set_property(GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE} ${GENERATED_BUILTINS_CPPS_${MODE}})
        add_custom_target(${target_name}_${MODE} DEPENDS ${BUILTINS_COMMANDS})
        add_dependencies(${target_name} ${target_name}_${MODE})
        set_target_properties(${target_name}_${MODE} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUIILINS_PROJECTS_FOLDER}/${family_name_with_type}")
      endforeach()
    endif()
  endforeach()
endmacro()

file(MAKE_DIRECTORY "${BUILTINS_OUTDIR_WITH_ARCH}/spirv")
foreach(builtin ${GENERATED_BUILTINS})
  generate_cpp_spirv(${builtin})
endforeach()
foreach(builtin_images ${GENERATED_BUILTINS_IMAGES})
  generate_cpp_spirv(${builtin_images})
endforeach()
foreach(builtin_stateless ${GENERATED_BUILTINS_STATELESS})
  generate_cpp_spirv(${builtin_stateless})
endforeach()
foreach(builtin_images_stateless ${GENERATED_BUILTINS_IMAGES_STATELESS})
  generate_cpp_spirv(${builtin_images_stateless})
endforeach()
add_library(${BUILTINS_SPIRV_LIB_NAME} OBJECT ${OUTPUT_LIST_CPP_FILES})
set_target_properties(${BUILTINS_SPIRV_LIB_NAME} PROPERTIES
                      POSITION_INDEPENDENT_CODE ON
                      FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUIILINS_PROJECTS_FOLDER}"
)

apply_macro_for_each_gen("SUPPORTED")
