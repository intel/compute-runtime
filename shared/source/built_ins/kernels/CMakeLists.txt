#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(builtins)
set_target_properties(builtins PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_STATELESS_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} builtins)
add_subdirectories()
set(GENERATED_BUILTINS ${GENERATED_BUILTINS} PARENT_SCOPE)
set(GENERATED_BUILTINS_AUX_TRANSLATION ${GENERATED_BUILTINS_AUX_TRANSLATION} PARENT_SCOPE)
set(GENERATED_BUILTINS_IMAGES ${GENERATED_BUILTINS_IMAGES} PARENT_SCOPE)
set(GENERATED_BUILTINS_STATELESS ${GENERATED_BUILTINS_STATELESS} PARENT_SCOPE)

set(BUILTIN_OPTIONS_STATELESS
    "-cl-intel-greater-than-4GB-buffer-required"
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  list(APPEND __ocloc__options__ "-D DEBUG")
endif()

function(get_bits_for_stateless core_type platform_type)
  # Force 32bits compiling on gen9lp for stateless builtins
  if((${core_type} STREQUAL "GEN9") AND (${platform_type} STREQUAL "LP"))
    set(BITS "32" PARENT_SCOPE)
  else()
    set(BITS ${NEO_BITS} PARENT_SCOPE)
  endif()
endfunction()

function(get_builtin_options core_type neo_arch)
  if("${neo_arch}" STREQUAL "x32")
    set(BUILTIN_OPTIONS "-cl-intel-greater-than-4GB-buffer-required" PARENT_SCOPE)
  elseif("${core_type}" STREQUAL "XE_HPC_CORE")
    set(BUILTIN_OPTIONS "" PARENT_SCOPE)
  else()
    set(BUILTIN_OPTIONS "-force_stos_opt" PARENT_SCOPE)
  endif()
endfunction()

# Define function for compiling built-ins (with ocloc)
function(compile_builtin core_type platform_type builtin bits builtin_options mode)
  string(TOLOWER ${core_type} core_type_lower)
  get_family_name_with_type(${core_type} ${platform_type})
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${core_type_lower}")
  # get filename
  set(FILENAME ${builtin})
  # get name of the file w/o extension
  get_filename_component(BASENAME ${builtin} NAME_WE)
  get_filename_component(absolute_filepath ${builtin} ABSOLUTE)

  set(OUTPUT_FILE_SPV
      ${OUTPUTDIR}/${mode}_${BASENAME}_${family_name_with_type}.spv
  )
  list(APPEND __ocloc__options__ "-cl-kernel-arg-info")
  set(INTERNAL_OPTIONS "${${mode}_OPTIONS}")
  add_custom_command(
                     OUTPUT ${OUTPUT_FILE_SPV}
                     COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -spv_only -device ${DEFAULT_SUPPORTED_${core_type}_${platform_type}_PLATFORM} ${builtin_options} -${bits} -output ${mode}_${BASENAME} -out_dir ${OUTPUTDIR} ${INTERNAL_OPTIONS} -options "$<JOIN:${__ocloc__options__}, >"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${builtin} ocloc copy_compiler_files
  )
  foreach(REVISION_ID ${${platform_type}_${core_type}_REVISIONS})
    set(OUTPUT_FILE_CPP
        ${OUTPUTDIR}/${mode}_${BASENAME}_${family_name_with_type}_${REVISION_ID}.cpp
    )
    set(BINARY_OUTPUT "${OUTPUTDIR}/${mode}_${BASENAME}_${REVISION_ID}_${family_name_with_type}")
    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(OUTPUT_FILES_BINARIES
          ${BINARY_OUTPUT}.gen
          ${BINARY_OUTPUT}.bin
      )
      get_filename_component(absolute_filepath_spv ${OUTPUT_FILE_SPV} ABSOLUTE)
      add_custom_command(
                         OUTPUT ${OUTPUT_FILES_BINARIES}
                         COMMAND ${ocloc_cmd_prefix} -q -gen_file -file ${absolute_filepath_spv} -spirv_input -device ${DEFAULT_SUPPORTED_${core_type}_${platform_type}_PLATFORM} ${builtin_options} -${bits} -output ${mode}_${BASENAME}_${REVISION_ID} -out_dir ${OUTPUTDIR} -revision_id ${REVISION_ID} ${INTERNAL_OPTIONS} -options "$<JOIN:${__ocloc__options__}, >"
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                         DEPENDS ${OUTPUT_FILE_SPV} ocloc copy_compiler_files
      )
      add_custom_command(
                         OUTPUT ${OUTPUT_FILE_CPP}
                         COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${BINARY_OUTPUT}.gen --output ${OUTPUT_FILE_CPP} --array ${mode}_${BASENAME} --platform ${family_name_with_type} --revision_id ${REVISION_ID}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                         DEPENDS ${OUTPUT_FILES_BINARIES} $<TARGET_FILE:cpp_generate_tool>
      )
      list(APPEND BUILTINS_COMMANDS "${OUTPUT_FILE_CPP}")
    else()
      foreach(_file_name "gen" "bin")
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/${core_type_lower}/${mode}_${BASENAME}_${REVISION_ID}_${family_name_with_type}.${_file_name}")
        if(EXISTS ${_file_prebuilt})
          add_custom_command(
                             OUTPUT ${BINARY_OUTPUT}.${_file_name}
                             COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                             COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
          )
        endif()
      endforeach()
      set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/${core_type_lower}/${mode}_${BASENAME}_${family_name_with_type}_${REVISION_ID}.cpp")
      if(EXISTS ${_file_prebuilt})
        add_custom_command(
                           OUTPUT ${OUTPUT_FILE_CPP}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
        )
        list(APPEND BUILTINS_COMMANDS "${OUTPUT_FILE_CPP}")
      endif()
    endif()
  endforeach()
  set(BUILTINS_COMMANDS ${BUILTINS_COMMANDS} PARENT_SCOPE)
endfunction()

function(generate_cpp_spirv builtin)
  get_filename_component(BASENAME ${builtin} NAME_WE)
  get_filename_component(DIR ${builtin} DIRECTORY)

  set(INPUT_FILENAME ${builtin}.builtin_kernel)
  get_filename_component(absolute_filepath ${INPUT_FILENAME} ABSOLUTE)

  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/spirv/${DIR}")
  string(REPLACE "//" "/" OUTPUTDIR ${OUTPUTDIR})

  set(GENERATED_SPV_INPUT ${OUTPUTDIR}/${BASENAME}.spv)
  set(OUTPUT_FILE_CPP
      ${OUTPUTDIR}/${BASENAME}.cpp
  )
  if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
    add_custom_command(
                       OUTPUT ${GENERATED_SPV_INPUT}
                       COMMAND ${ocloc_cmd_prefix} -q -spv_only -file ${absolute_filepath} -out_dir ${OUTPUTDIR} -output_no_suffix -options "-cl-kernel-arg-info"
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${INPUT_FILENAME} ocloc copy_compiler_files
    )
    add_custom_command(
                       OUTPUT ${OUTPUT_FILE_CPP}
                       COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${GENERATED_SPV_INPUT} --output ${OUTPUT_FILE_CPP} --array ${BASENAME}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${GENERATED_SPV_INPUT} $<TARGET_FILE:cpp_generate_tool>
    )
    set(OUTPUT_LIST_CPP_FILES ${OUTPUT_LIST_CPP_FILES} ${OUTPUT_FILE_CPP} PARENT_SCOPE)
  else()
    set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/spirv/${DIR}/${BASENAME}.spv")
    add_custom_command(
                       OUTPUT ${GENERATED_SPV_INPUT}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
    )
    set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/spirv/${DIR}/${BASENAME}.cpp")
    add_custom_command(
                       OUTPUT ${OUTPUT_FILE_CPP}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
                       DEPENDS ${GENERATED_SPV_INPUT}
    )
    set(OUTPUT_LIST_CPP_FILES ${OUTPUT_LIST_CPP_FILES} ${OUTPUT_FILE_CPP} PARENT_SCOPE)
  endif()
endfunction()

macro(macro_for_each_core_type)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${CORE_TYPE}_HAS_${PLATFORM_TYPE})
      unset(IMAGE_SUPPORT)
      unset(AUX_TRANSLATION_SUPPORT)
      CORE_CONTAINS_PLATFORMS("SUPPORTED_IMAGES" ${CORE_TYPE} IMAGE_SUPPORT)
      CORE_CONTAINS_PLATFORMS("SUPPORTED_AUX_TRANSLATION" ${CORE_TYPE} AUX_TRANSLATION_SUPPORT)
      get_family_name_with_type(${CORE_TYPE} ${PLATFORM_TYPE})
      string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
      get_bits_for_stateless(${CORE_TYPE} ${PLATFORM_TYPE})
      get_builtin_options(${CORE_TYPE} ${NEO_ARCH})
      set(target_name builtins_${family_name_with_type})
      add_custom_target(${target_name})
      add_dependencies(builtins ${target_name})
      set_target_properties(${target_name} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}/${family_name_with_type}")
      foreach(MODE ${ADDRESSING_MODES})
        unset(BUILTINS_COMMANDS)

        if((${MODE} STREQUAL "bindless") AND (NOT BUILD_WITH_L0 OR("${CORE_TYPE}" STREQUAL "GEN8")))
          continue()
        endif()

        if(${AUX_TRANSLATION_SUPPORT})
          foreach(GENERATED_BUILTIN_AUX_TRANSLATION ${GENERATED_BUILTINS_AUX_TRANSLATION})
            compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN_AUX_TRANSLATION}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
          endforeach()
        endif()

        if(${MODE} STREQUAL "stateless")
          foreach(GENERATED_BUILTIN_STATELESS ${GENERATED_BUILTINS_STATELESS})
            compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN_STATELESS}.builtin_kernel ${BITS} "${BUILTIN_OPTIONS_STATELESS}" ${MODE})
          endforeach()

          if(${IMAGE_SUPPORT})
            foreach(GENERATED_BUILTIN_IMAGES_STATELESS ${GENERATED_BUILTINS_IMAGES_STATELESS})
              compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN_IMAGES_STATELESS}.builtin_kernel ${BITS} "${BUILTIN_OPTIONS_STATELESS}" ${MODE})
            endforeach()
          endif()

          if("${CORE_TYPE}" STREQUAL "XE_HPC_CORE")
            # Compile stateful versions as stateless
            foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
              compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
            endforeach()

            if(${IMAGE_SUPPORT})
              foreach(GENERATED_BUILTINS_IMAGES ${GENERATED_BUILTINS_IMAGES})
                compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTINS_IMAGES}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
              endforeach()
            endif()
          endif()
        else()
          if("${CORE_TYPE}" STREQUAL "XE_HPC_CORE")
            continue()
          endif()
          foreach(GENERATED_BUILTIN ${GENERATED_BUILTINS})
            compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTIN}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
          endforeach()

          if(${IMAGE_SUPPORT})
            foreach(GENERATED_BUILTINS_IMAGES ${GENERATED_BUILTINS_IMAGES})
              compile_builtin(${CORE_TYPE} ${PLATFORM_TYPE} ${GENERATED_BUILTINS_IMAGES}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE})
            endforeach()
          endif()
        endif()

        get_property(GENERATED_BUILTINS_CPPS_${MODE} GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE})
        foreach(BUILTIN ${BUILTINS_COMMANDS})
          list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTIN})
        endforeach()
        set_property(GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE} ${GENERATED_BUILTINS_CPPS_${MODE}})
        add_custom_target(${target_name}_${MODE} DEPENDS ${BUILTINS_COMMANDS})
        add_dependencies(${target_name} ${target_name}_${MODE})
        set_target_properties(${target_name}_${MODE} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}/${family_name_with_type}")
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
foreach(builtin_aux_translation ${GENERATED_BUILTINS_AUX_TRANSLATION})
  generate_cpp_spirv(${builtin_aux_translation})
endforeach()
foreach(builtin_stateless ${GENERATED_BUILTINS_STATELESS})
  generate_cpp_spirv(${builtin_stateless})
endforeach()
foreach(builtin_images_stateless ${GENERATED_BUILTINS_IMAGES_STATELESS})
  generate_cpp_spirv(${builtin_images_stateless})
endforeach()
if(NOT "${OUTPUT_LIST_CPP_FILES}" STREQUAL "")
  add_library(${BUILTINS_SPIRV_LIB_NAME} OBJECT ${OUTPUT_LIST_CPP_FILES})
  target_compile_definitions(${BUILTINS_SPIRV_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  set_target_properties(${BUILTINS_SPIRV_LIB_NAME} PROPERTIES
                        POSITION_INDEPENDENT_CODE ON
                        FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}"

  )
endif()

apply_macro_for_each_core_type("SUPPORTED")
