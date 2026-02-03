#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(builtins)
set_target_properties(builtins PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_STATELESS_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_WIDE_STATELESS_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_STATELESS_HEAPLESS_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_WIDE_STATELESS_HEAPLESS_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} builtins)
add_dependencies(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} builtins)

set(GENERATED_BUILTINS ${GENERATED_BUILTINS} PARENT_SCOPE)
set(GENERATED_BUILTINS_AUX_TRANSLATION ${GENERATED_BUILTINS_AUX_TRANSLATION} PARENT_SCOPE)
set(GENERATED_BUILTINS_IMAGES ${GENERATED_BUILTINS_IMAGES} PARENT_SCOPE)
set(GENERATED_BUILTINS_STATELESS ${GENERATED_BUILTINS_STATELESS} PARENT_SCOPE)

set(BUILTIN_OPTIONS_STATELESS "-cl-intel-greater-than-4GB-buffer-required")
set(BUILTIN_OPTIONS_STATEFUL "-force_stos_opt")

set(stateless_INTERNAL_OPTIONS "")
set(wide_stateless_INTERNAL_OPTIONS "")
set(stateless_heapless_INTERNAL_OPTIONS -internal_options "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode -ze-intel-64bit-addressing")
set(wide_stateless_heapless_INTERNAL_OPTIONS ${stateless_heapless_INTERNAL_OPTIONS})
set(bindful_INTERNAL_OPTIONS "")
set(bindless_INTERNAL_OPTIONS -internal_options "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode")
macro(additional_heapless_checker)
endmacro()

add_subdirectories()

function(get_builtin_options mode_type)
  if(${mode_type} IN_LIST STATELESS_ADDRESSING_MODES)
    set(BUILTIN_OPTIONS "${BUILTIN_OPTIONS_STATELESS}" PARENT_SCOPE)
  else()
    set(BUILTIN_OPTIONS "${BUILTIN_OPTIONS_STATEFUL}" PARENT_SCOPE)
  endif()
endfunction()

function(get_generated_builtins core_type mode_type)
  if("${core_type}" STREQUAL "XE_HPC_CORE")
    # Compile all builtin kernels as stateless for XeHpc
    set(generated_builtins "${GENERATED_BUILTINS}" PARENT_SCOPE)
    set(generated_builtins_stateless "${GENERATED_BUILTINS_STATELESS}" PARENT_SCOPE)
    set(generated_builtins_images "${GENERATED_BUILTINS_IMAGES}" PARENT_SCOPE)
    set(generated_builtins_images_stateless "${GENERATED_BUILTINS_IMAGES_STATELESS}" PARENT_SCOPE)
  elseif(${mode_type} STREQUAL "stateless" OR ${mode_type} STREQUAL "wide_stateless")
    set(generated_builtins "" PARENT_SCOPE)
    set(generated_builtins_stateless "${GENERATED_BUILTINS_STATELESS}" PARENT_SCOPE)
    set(generated_builtins_images "" PARENT_SCOPE)
    set(generated_builtins_images_stateless "${GENERATED_BUILTINS_IMAGES_STATELESS}" PARENT_SCOPE)
  elseif(${mode_type} IN_LIST HEAPLESS_ADDRESSING_MODES)
    set(generated_builtins "" PARENT_SCOPE)
    set(generated_builtins_stateless "${GENERATED_BUILTINS_STATELESS}" PARENT_SCOPE)
    set(generated_builtins_images "${GENERATED_BUILTINS_IMAGES}" PARENT_SCOPE)
    set(generated_builtins_images_stateless "${GENERATED_BUILTINS_IMAGES_STATELESS}" PARENT_SCOPE)
  else()
    set(generated_builtins "${GENERATED_BUILTINS}" PARENT_SCOPE)
    set(generated_builtins_stateless "" PARENT_SCOPE)
    set(generated_builtins_images "${GENERATED_BUILTINS_IMAGES}" PARENT_SCOPE)
    set(generated_builtins_images_stateless "" PARENT_SCOPE)
  endif()
endfunction()

# Define function for compiling built-ins (with ocloc)
function(compile_builtin core_type platform_it builtin bits builtin_options mode is_stateless)
  string(TOLOWER ${platform_it} platform_it_lower)
  string(TOLOWER ${core_type} core_type_lower)
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${core_type_lower}")
  # get filename
  set(FILENAME ${builtin})
  # get name of the file w/o extension
  get_filename_component(BASENAME ${builtin} NAME_WE)
  get_filename_component(absolute_filepath ${builtin} ABSOLUTE)

  if(is_stateless)
    set(BASENAME ${BASENAME}_stateless)
  endif()

  set(OUTPUT_FILE_SPV
      ${OUTPUTDIR}/${mode}_${BASENAME}_${platform_it_lower}.spv
  )
  set(TEMP_OUTPUT_FILE_SPV
      ${OUTPUTDIR}/__${mode}_${BASENAME}_${platform_it_lower}.spv
  )

  set(heapless_mode "disable")
  if(${mode} IN_LIST HEAPLESS_ADDRESSING_MODES)
    set(heapless_mode "enable")
  endif()

  if(${mode} STREQUAL "wide_stateless" OR ${mode} STREQUAL "wide_stateless_heapless")
    list(APPEND __ocloc__options__ ${WIDE_STATELESS_TYPES})
  else()
    list(APPEND __ocloc__options__ ${KERNEL_TYPES})
  endif()
  list(APPEND __ocloc__options__ "-cl-kernel-arg-info")

  set(INTERNAL_OPTIONS "${${mode}_INTERNAL_OPTIONS}")
  add_custom_command(
                     OUTPUT ${OUTPUT_FILE_SPV}
                     COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -spv_only -device ${platform_it_lower} -heapless_mode ${heapless_mode} ${builtin_options} -${bits} -output __${mode}_${BASENAME} -out_dir ${OUTPUTDIR} ${INTERNAL_OPTIONS} -options "$<JOIN:${__ocloc__options__}, >"
                     COMMAND ${CMAKE_COMMAND} -E rename ${TEMP_OUTPUT_FILE_SPV} ${OUTPUT_FILE_SPV}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${builtin} ocloc copy_compiler_files
  )

  set(stateful_address_mode "default")
  if(${mode} STREQUAL "bindless")
    set(stateful_address_mode "bindless")
  elseif(${mode} STREQUAL "bindful")
    set(stateful_address_mode "bindful")
  elseif(${mode} STREQUAL "stateless" OR ${mode} STREQUAL "wide_stateless")
    set(stateful_address_mode "bindful")
  endif()

  set(OUTPUT_FILES_BINARIES_PREV)

  foreach(RELEASE ${${platform_it}_${core_type}_RELEASES})
    string(REPLACE "." "_" RELEASE_FILENAME ${RELEASE})
    set(OUTPUT_FILE_CPP
        ${OUTPUTDIR}/${mode}_${BASENAME}_${RELEASE_FILENAME}.cpp
    )
    set(TEMP_OUTPUT_FILE_CPP
        ${OUTPUTDIR}/__${mode}_${BASENAME}_${RELEASE_FILENAME}.cpp
    )
    get_property(RELEASE_HANDLED SOURCE ${OUTPUT_FILE_CPP} PROPERTY GENERATED)
    if(RELEASE_HANDLED)
      continue()
    endif()
    set(BINARY_OUTPUT "${OUTPUTDIR}/${mode}_${BASENAME}_${RELEASE_FILENAME}")
    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(OUTPUT_FILES_BINARIES ${BINARY_OUTPUT}.bin)
      get_filename_component(absolute_filepath_spv ${OUTPUT_FILE_SPV} ABSOLUTE)
      add_custom_command(
                         OUTPUT ${OUTPUT_FILE_CPP}
                         COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath_spv} -spirv_input -device ${RELEASE} -heapless_mode ${heapless_mode} ${builtin_options} -${bits} -stateful_address_mode ${stateful_address_mode} -output ${mode}_${BASENAME}_${RELEASE_FILENAME} -output_no_suffix -out_dir ${OUTPUTDIR} ${INTERNAL_OPTIONS} -options "$<JOIN:${__ocloc__options__}, >"
                         COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${BINARY_OUTPUT}.bin --output ${TEMP_OUTPUT_FILE_CPP} --array ${mode}_${BASENAME} --device ${RELEASE_FILENAME}
                         COMMAND ${CMAKE_COMMAND} -E rename ${TEMP_OUTPUT_FILE_CPP} ${OUTPUT_FILE_CPP}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                         DEPENDS ${OUTPUT_FILE_SPV} ocloc copy_compiler_files ${OUTPUT_FILES_BINARIES_PREV} cpp_generate_tool
      )
      if(NEO_SERIALIZED_BUILTINS_COMPILATION)
        set(OUTPUT_FILES_BINARIES_PREV ${OUTPUT_FILE_CPP})
      endif()
      list(APPEND BUILTINS_COMMANDS "${OUTPUT_FILE_CPP}")
    else()
      foreach(_file_name "bin")
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/${core_type_lower}/${mode}_${BASENAME}_${RELEASE_FILENAME}.${_file_name}")
        if(EXISTS ${_file_prebuilt})
          add_custom_command(
                             OUTPUT ${BINARY_OUTPUT}.${_file_name}
                             COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                             COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
          )
        endif()
      endforeach()
      set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/${core_type_lower}/${mode}_${BASENAME}_${RELEASE_FILENAME}.cpp")
      add_custom_command(
                         OUTPUT ${OUTPUT_FILE_CPP}
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
      )
      list(APPEND BUILTINS_COMMANDS "${OUTPUT_FILE_CPP}")
    endif()
  endforeach()
  set(BUILTINS_COMMANDS ${BUILTINS_COMMANDS} PARENT_SCOPE)
endfunction()

function(generate_cpp_spirv builtin mode)
  get_filename_component(BASENAME ${builtin} NAME_WE)
  if(${mode} STREQUAL "stateless" OR ${mode} STREQUAL "wide_stateless")
    set(KERNEL_INTERNAL_OPTIONS "-cl-intel-greater-than-4GB-buffer-required")
    set(BASENAME ${BASENAME}_stateless)
  endif()

  if(${mode} STREQUAL "wide_stateless")
    set(BASENAME wide_stateless_${BASENAME})
  endif()

  set(INPUT_FILENAME ${builtin}.builtin_kernel)
  get_filename_component(absolute_filepath ${INPUT_FILENAME} ABSOLUTE)

  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/spirv/")
  string(REPLACE "//" "/" OUTPUTDIR ${OUTPUTDIR})

  set(GENERATED_SPV_INPUT ${OUTPUTDIR}/${BASENAME}.spv)

  set(OUTPUT_FILE_CPP
      ${OUTPUTDIR}/${BASENAME}.cpp
  )
  set(TEMP_OUTPUT_FILE_CPP
      ${OUTPUTDIR}/__${BASENAME}.cpp
  )

  if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
    if(${mode} STREQUAL "wide_stateless")
      set(KERNEL_OPTIONS ${WIDE_STATELESS_TYPES})
    else()
      set(KERNEL_OPTIONS ${KERNEL_TYPES})
    endif()
    set(KERNEL_OPTIONS "${KERNEL_OPTIONS} -cl-kernel-arg-info")

    add_custom_command(
                       OUTPUT ${OUTPUT_FILE_CPP}
                       COMMAND ${ocloc_cmd_prefix} -q -spv_only -file "${absolute_filepath}" -out_dir "${OUTPUTDIR}" -output "${BASENAME}" ${KERNEL_INTERNAL_OPTIONS} -output_no_suffix -options "${KERNEL_OPTIONS}"
                       COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${GENERATED_SPV_INPUT} --output ${TEMP_OUTPUT_FILE_CPP} --array ${BASENAME}
                       COMMAND ${CMAKE_COMMAND} -E rename ${TEMP_OUTPUT_FILE_CPP} ${OUTPUT_FILE_CPP}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${INPUT_FILENAME} ocloc copy_compiler_files ${OUTPUT_FILES_SPIRV_PREV} cpp_generate_tool
    )
    if(NEO_SERIALIZED_BUILTINS_COMPILATION)
      set(OUTPUT_FILES_SPIRV_PREV ${OUTPUT_FILE_CPP})
    endif()
    set(OUTPUT_LIST_CPP_FILES ${OUTPUT_LIST_CPP_FILES} ${OUTPUT_FILE_CPP} PARENT_SCOPE)
  else()
    set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/spirv/${BASENAME}.spv")
    add_custom_command(
                       OUTPUT ${GENERATED_SPV_INPUT}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
    )
    set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/spirv/${BASENAME}.cpp")
    add_custom_command(
                       OUTPUT ${OUTPUT_FILE_CPP}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
                       DEPENDS ${GENERATED_SPV_INPUT}
    )
    set(OUTPUT_LIST_CPP_FILES ${OUTPUT_LIST_CPP_FILES} ${OUTPUT_FILE_CPP} PARENT_SCOPE)
  endif()
endfunction()

macro(macro_for_each_platform)
  set(IMAGE_SUPPORT FALSE)
  set(AUX_TRANSLATION_SUPPORT FALSE)
  set(HEAPLESS_SUPPORT FALSE)

  CORE_CONTAINS_PLATFORM("SUPPORTED_IMAGES" ${CORE_TYPE} ${PLATFORM_IT} IMAGE_SUPPORT)
  CORE_CONTAINS_PLATFORM("SUPPORTED_AUX_TRANSLATION" ${CORE_TYPE} ${PLATFORM_IT} AUX_TRANSLATION_SUPPORT)
  CORE_CONTAINS_PLATFORM("SUPPORTED_HEAPLESS" ${CORE_TYPE} ${PLATFORM_IT} HEAPLESS_SUPPORT)

  set(target_name builtins_${PLATFORM_IT_LOWER})
  add_custom_target(${target_name})
  add_dependencies(builtins ${target_name})
  set_target_properties(${target_name} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}/${PLATFORM_IT_LOWER}")

  foreach(MODE ${ADDRESSING_MODES})
    unset(BUILTINS_COMMANDS)

    additional_heapless_checker()

    if((${MODE} STREQUAL "bindless") AND (NOT BUILD_WITH_L0 OR("${CORE_TYPE}" STREQUAL "GEN8")))
      continue()
    endif()

    # XeHpc supports only stateless addressing mode
    if(NOT (${MODE} STREQUAL "stateless" OR ${MODE} STREQUAL "wide_stateless") AND "${CORE_TYPE}" STREQUAL "XE_HPC_CORE")
      continue()
    endif()

    if(${MODE} IN_LIST HEAPLESS_ADDRESSING_MODES AND(NOT ${HEAPLESS_SUPPORT} OR ${NEO_BITS} STREQUAL "32"))
      continue()
    endif()

    get_builtin_options(${MODE})
    get_generated_builtins(${CORE_TYPE} ${MODE})

    if(${AUX_TRANSLATION_SUPPORT})
      # Stateless aux translation are only supported for XeHpc
      if(NOT ${MODE} STREQUAL "stateless" OR "${CORE_TYPE}" STREQUAL "XE_HPC_CORE")
        foreach(GENERATED_BUILTIN_AUX_TRANSLATION ${GENERATED_BUILTINS_AUX_TRANSLATION})
          compile_builtin(${CORE_TYPE} ${PLATFORM_IT} ${GENERATED_BUILTIN_AUX_TRANSLATION}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE} FALSE)
        endforeach()
      endif()
    endif()

    foreach(generated_builtin ${generated_builtins})
      compile_builtin(${CORE_TYPE} ${PLATFORM_IT} ${generated_builtin}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE} FALSE)
    endforeach()
    foreach(generated_builtin ${generated_builtins_stateless})
      compile_builtin(${CORE_TYPE} ${PLATFORM_IT} ${generated_builtin}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE} TRUE)
    endforeach()

    if(${IMAGE_SUPPORT})
      foreach(generated_builtin_images ${generated_builtins_images})
        compile_builtin(${CORE_TYPE} ${PLATFORM_IT} ${generated_builtin_images}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE} FALSE)
      endforeach()
      foreach(generated_builtin_images ${generated_builtins_images_stateless})
        compile_builtin(${CORE_TYPE} ${PLATFORM_IT} ${generated_builtin_images}.builtin_kernel ${NEO_BITS} "${BUILTIN_OPTIONS}" ${MODE} TRUE)
      endforeach()
    endif()

    get_property(GENERATED_BUILTINS_CPPS_${MODE} GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE})
    foreach(BUILTIN ${BUILTINS_COMMANDS})
      list(APPEND GENERATED_BUILTINS_CPPS_${MODE} ${BUILTIN})
    endforeach()
    set_property(GLOBAL PROPERTY GENERATED_BUILTINS_CPPS_${MODE} ${GENERATED_BUILTINS_CPPS_${MODE}})
    add_custom_target(${target_name}_${MODE} DEPENDS ${BUILTINS_COMMANDS})
    add_dependencies(${target_name} ${target_name}_${MODE})
    set_target_properties(${target_name}_${MODE} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}/${PLATFORM_IT_LOWER}")
  endforeach()

endmacro()

macro(macro_for_each_core_type)
  apply_macro_for_each_platform("SUPPORTED")
endmacro()

file(MAKE_DIRECTORY "${BUILTINS_OUTDIR_WITH_ARCH}/spirv")

set(OUTPUT_FILES_SPIRV_PREV)

foreach(MODE ${SPIRV_ADDRESSING_MODES})
  get_generated_builtins("" ${MODE})

  foreach(generated_builtin ${generated_builtins})
    generate_cpp_spirv(${generated_builtin} ${MODE})
  endforeach()
  foreach(generated_builtin ${generated_builtins_stateless})
    generate_cpp_spirv(${generated_builtin} ${MODE})
  endforeach()

  foreach(generated_builtin_images ${generated_builtins_images})
    generate_cpp_spirv(${generated_builtin_images} ${MODE})
  endforeach()
  foreach(generated_builtin_images ${generated_builtins_images_stateless})
    generate_cpp_spirv(${generated_builtin_images} ${MODE})
  endforeach()

  if(${MODE} STREQUAL "stateful")
    foreach(builtin_aux_translation ${GENERATED_BUILTINS_AUX_TRANSLATION})
      generate_cpp_spirv(${builtin_aux_translation} ${MODE})
    endforeach()
  endif()
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
