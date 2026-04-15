#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(builtins)
set_target_properties(builtins PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}")
set(BUILTINS_OUTDIR_WITH_ARCH "${TargetDir}/built_ins/${NEO_ARCH}")
add_dependencies(${BUILTINS_BINARIES_LIB_NAME} builtins)

set(BUILTINS_AUX_TRANSLATION ${BUILTINS_AUX_TRANSLATION} PARENT_SCOPE)
set(BUILTINS_BUFFER ${BUILTINS_BUFFER} PARENT_SCOPE)
set(BUILTINS_IMAGE ${BUILTINS_IMAGE} PARENT_SCOPE)
set(BUILTINS_IMAGE_BUFFER ${BUILTINS_IMAGE_BUFFER} PARENT_SCOPE)

set(BUILTIN_OPTIONS_STATELESS "-cl-intel-greater-than-4GB-buffer-required")
set(BUILTIN_OPTIONS_STATEFUL "-force_stos_opt")
set(USE_INTRINSICS_WB_POLICY "-DUSE_LSC_INTRINSICS_WB")

add_subdirectories()

function(compile_builtin_with_prefix core_type platform_it builtin bits builtin_options
           mode_prefix stateful_addr_mode heapless_flag internal_options kernel_types
  )
  string(TOLOWER ${platform_it} platform_it_lower)
  string(TOLOWER ${core_type} core_type_lower)
  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/${core_type_lower}")

  get_filename_component(BASENAME ${builtin} NAME_WE)
  get_filename_component(absolute_filepath ${builtin} ABSOLUTE)

  set(OUTPUT_FILE_SPV
      ${OUTPUTDIR}/${mode_prefix}_${BASENAME}_${platform_it_lower}.spv
  )
  set(TEMP_OUTPUT_FILE_SPV
      ${OUTPUTDIR}/__${mode_prefix}_${BASENAME}_${platform_it_lower}.spv
  )

  set(__ocloc__options__ "")
  foreach(kernel_type ${kernel_types})
    list(APPEND __ocloc__options__ ${kernel_type})
  endforeach()

  if(${platform_it_lower} STREQUAL "bmg")
    list(APPEND __ocloc__options__ ${LSC_STORE_INTRINSICS})
    list(APPEND __ocloc__options__ ${USE_INTRINSICS_WB_POLICY})
  endif()
  list(APPEND __ocloc__options__ "-cl-kernel-arg-info")

  add_custom_command(
                     OUTPUT ${OUTPUT_FILE_SPV}
                     COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -spv_only -device ${platform_it_lower} -heapless_mode ${heapless_flag} ${builtin_options} -${bits} -output __${mode_prefix}_${BASENAME} -out_dir ${OUTPUTDIR} ${internal_options} -options "$<JOIN:${__ocloc__options__}, >"
                     COMMAND ${CMAKE_COMMAND} -E rename ${TEMP_OUTPUT_FILE_SPV} ${OUTPUT_FILE_SPV}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${builtin} ocloc copy_compiler_files
  )

  set(OUTPUT_FILES_BINARIES_PREV)

  foreach(RELEASE ${${platform_it}_${core_type}_RELEASES})
    string(REPLACE "." "_" RELEASE_FILENAME ${RELEASE})
    set(OUTPUT_FILE_CPP
        ${OUTPUTDIR}/${mode_prefix}_${BASENAME}_${RELEASE_FILENAME}.cpp
    )
    set(TEMP_OUTPUT_FILE_CPP
        ${OUTPUTDIR}/__${mode_prefix}_${BASENAME}_${RELEASE_FILENAME}.cpp
    )
    get_property(RELEASE_HANDLED SOURCE ${OUTPUT_FILE_CPP} PROPERTY GENERATED)
    if(RELEASE_HANDLED)
      continue()
    endif()
    set(BINARY_OUTPUT "${OUTPUTDIR}/${mode_prefix}_${BASENAME}_${RELEASE_FILENAME}")
    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(OUTPUT_FILES_BINARIES ${BINARY_OUTPUT}.bin)
      get_filename_component(absolute_filepath_spv ${OUTPUT_FILE_SPV} ABSOLUTE)
      add_custom_command(
                         OUTPUT ${OUTPUT_FILE_CPP}
                         COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath_spv} -spirv_input -device ${RELEASE} -heapless_mode ${heapless_flag} ${builtin_options} -${bits} -stateful_address_mode ${stateful_addr_mode} -output ${mode_prefix}_${BASENAME}_${RELEASE_FILENAME} -output_no_suffix -out_dir ${OUTPUTDIR} ${internal_options} -options "$<JOIN:${__ocloc__options__}, >"
                         COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${BINARY_OUTPUT}.bin --output ${TEMP_OUTPUT_FILE_CPP} --array ${mode_prefix}_${BASENAME} --device ${RELEASE_FILENAME}
                         COMMAND ${CMAKE_COMMAND} -E rename ${TEMP_OUTPUT_FILE_CPP} ${OUTPUT_FILE_CPP}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                         DEPENDS ${OUTPUT_FILE_SPV} ocloc copy_compiler_files ${OUTPUT_FILES_BINARIES_PREV} cpp_generate_tool
      )
      if(NEO_SERIALIZED_BUILTINS_COMPILATION)
        set(OUTPUT_FILES_BINARIES_PREV ${OUTPUT_FILE_CPP})
      endif()
      list(APPEND BUILTINS_COMMANDS "${OUTPUT_FILE_CPP}")
    else()
      set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/${core_type_lower}/${mode_prefix}_${BASENAME}_${RELEASE_FILENAME}.bin")
      if(EXISTS ${_file_prebuilt})
        add_custom_command(
                           OUTPUT ${BINARY_OUTPUT}.bin
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
        )
      endif()
      set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/${core_type_lower}/${mode_prefix}_${BASENAME}_${RELEASE_FILENAME}.cpp")
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

function(generate_cpp_spirv builtin mode_prefix builtin_options is_wide)
  get_filename_component(BASENAME ${builtin} NAME_WE)

  set(INPUT_FILENAME ${builtin}.builtin_kernel)
  get_filename_component(absolute_filepath ${INPUT_FILENAME} ABSOLUTE)

  set(OUTPUTDIR "${BUILTINS_OUTDIR_WITH_ARCH}/spirv/")
  string(REPLACE "//" "/" OUTPUTDIR ${OUTPUTDIR})

  set(GENERATED_SPV_INPUT ${OUTPUTDIR}/${mode_prefix}_${BASENAME}.spv)

  set(OUTPUT_FILE_CPP
      ${OUTPUTDIR}/${mode_prefix}_${BASENAME}.cpp
  )
  set(TEMP_OUTPUT_FILE_CPP
      ${OUTPUTDIR}/__${mode_prefix}_${BASENAME}.cpp
  )

  if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
    if(${is_wide})
      set(KERNEL_OPTIONS ${WIDE_STATELESS_TYPES})
    else()
      set(KERNEL_OPTIONS ${KERNEL_TYPES})
    endif()
    set(KERNEL_OPTIONS "${KERNEL_OPTIONS} -cl-kernel-arg-info")

    add_custom_command(
                       OUTPUT ${OUTPUT_FILE_CPP}
                       COMMAND ${ocloc_cmd_prefix} -q -spv_only -file "${absolute_filepath}" -out_dir "${OUTPUTDIR}" -output "${mode_prefix}_${BASENAME}" ${builtin_options} -output_no_suffix -options "${KERNEL_OPTIONS}"
                       COMMAND $<TARGET_FILE:cpp_generate_tool> --file ${GENERATED_SPV_INPUT} --output ${TEMP_OUTPUT_FILE_CPP} --array ${mode_prefix}_${BASENAME}
                       COMMAND ${CMAKE_COMMAND} -E rename ${TEMP_OUTPUT_FILE_CPP} ${OUTPUT_FILE_CPP}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       DEPENDS ${INPUT_FILENAME} ocloc copy_compiler_files ${OUTPUT_FILES_SPIRV_PREV} cpp_generate_tool
    )
    if(NEO_SERIALIZED_BUILTINS_COMPILATION)
      set(OUTPUT_FILES_SPIRV_PREV ${OUTPUT_FILE_CPP})
    endif()
    set(OUTPUT_LIST_CPP_FILES ${OUTPUT_LIST_CPP_FILES} ${OUTPUT_FILE_CPP} PARENT_SCOPE)
  else()
    set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/spirv/${mode_prefix}_${BASENAME}.spv")
    add_custom_command(
                       OUTPUT ${GENERATED_SPV_INPUT}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUTDIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${OUTPUTDIR}
    )
    set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/built_ins/${NEO_ARCH}/spirv/${mode_prefix}_${BASENAME}.cpp")
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

  CORE_CONTAINS_PLATFORM("SUPPORTED_IMAGES" ${CORE_TYPE} ${PLATFORM_IT} IMAGE_SUPPORT)
  CORE_CONTAINS_PLATFORM("SUPPORTED_AUX_TRANSLATION" ${CORE_TYPE} ${PLATFORM_IT} AUX_TRANSLATION_SUPPORT)

  set(target_name builtins_${PLATFORM_IT_LOWER})
  add_custom_target(${target_name})
  add_dependencies(builtins ${target_name})
  set_target_properties(${target_name} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}/${PLATFORM_IT_LOWER}")

  FIND_IDX_FOR_CORE_TYPE(${CORE_TYPE} CORE_IDX)
  GET_LIST_FOR_CORE_TYPE("ADDRESSING_MODES" "BUILTIN" ${CORE_IDX} PLATFORM_ADDRESSING_MODES)

  set(PLATFORM_HEAPLESS OFF)
  if("${CORE_TYPE}" IN_LIST HEAPLESS_CORE_TYPES)
    set(PLATFORM_HEAPLESS ON)
  endif()

  set(HEAPLESS_FLAG "disable")
  if(PLATFORM_HEAPLESS)
    set(HEAPLESS_FLAG "enable")
  endif()

  set(ALL_BUILTINS ${BUILTINS_BUFFER})
  if(${IMAGE_SUPPORT})
    list(APPEND ALL_BUILTINS ${BUILTINS_IMAGE} ${BUILTINS_IMAGE_BUFFER})
  endif()

  foreach(ADDR_MODE ${PLATFORM_ADDRESSING_MODES})
    unset(BUILTINS_COMMANDS)

    set(IS_BINDLESS FALSE)
    if(ADDR_MODE MATCHES "bindless")
      set(IS_BINDLESS TRUE)
    endif()

    set(IS_STATELESS FALSE)
    if(ADDR_MODE MATCHES "stateless")
      set(IS_STATELESS TRUE)
    endif()

    set(IS_WIDE FALSE)
    if(ADDR_MODE MATCHES "wide")
      set(IS_WIDE TRUE)
    endif()

    if(IS_STATELESS)
      set(BUILTIN_OPTIONS "${BUILTIN_OPTIONS_STATELESS}")
    else()
      set(BUILTIN_OPTIONS "${BUILTIN_OPTIONS_STATEFUL}")
    endif()

    if(IS_BINDLESS)
      set(STATEFUL_ADDR_MODE "bindless")
    else()
      set(STATEFUL_ADDR_MODE "bindful")
    endif()

    set(INTERNAL_OPTIONS "")
    if(PLATFORM_HEAPLESS)
      set(INTERNAL_OPTIONS -internal_options "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode -ze-intel-64bit-addressing")
    elseif(IS_BINDLESS)
      set(INTERNAL_OPTIONS -internal_options "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode")
    endif()

    if(IS_WIDE)
      set(ACTIVE_KERNEL_TYPES "${WIDE_STATELESS_TYPES}")
    else()
      set(ACTIVE_KERNEL_TYPES "${KERNEL_TYPES}")
    endif()

    # Aux translation
    if(${AUX_TRANSLATION_SUPPORT} AND NOT IS_WIDE)
      if(NOT IS_STATELESS OR "${CORE_TYPE}" STREQUAL "XE_HPC_CORE")
        foreach(KERNEL ${BUILTINS_AUX_TRANSLATION})
          compile_builtin_with_prefix(${CORE_TYPE} ${PLATFORM_IT} ${KERNEL}.builtin_kernel ${NEO_BITS}
                                      "${BUILTIN_OPTIONS}" "${ADDR_MODE}" "${STATEFUL_ADDR_MODE}"
                                      "${HEAPLESS_FLAG}" "${INTERNAL_OPTIONS}" "${ACTIVE_KERNEL_TYPES}"
          )
        endforeach()
      endif()
    endif()

    foreach(KERNEL ${ALL_BUILTINS})
      compile_builtin_with_prefix(${CORE_TYPE} ${PLATFORM_IT} ${KERNEL}.builtin_kernel ${NEO_BITS}
                                  "${BUILTIN_OPTIONS}" "${ADDR_MODE}" "${STATEFUL_ADDR_MODE}"
                                  "${HEAPLESS_FLAG}" "${INTERNAL_OPTIONS}" "${ACTIVE_KERNEL_TYPES}"
      )
    endforeach()

    get_property(GENERATED_BUILTINS_CPPS GLOBAL PROPERTY GENERATED_BUILTINS_CPPS)

    foreach(BUILTIN ${BUILTINS_COMMANDS})
      list(APPEND GENERATED_BUILTINS_CPPS ${BUILTIN})
    endforeach()

    set_property(GLOBAL PROPERTY GENERATED_BUILTINS_CPPS ${GENERATED_BUILTINS_CPPS})
    add_custom_target(${target_name}_${ADDR_MODE} DEPENDS ${BUILTINS_COMMANDS})
    add_dependencies(${target_name} ${target_name}_${ADDR_MODE})
    set_target_properties(${target_name}_${ADDR_MODE} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}/${SHARED_BUILTINS_PROJECTS_FOLDER}/${PLATFORM_IT_LOWER}")
  endforeach()

endmacro()

macro(macro_for_each_core_type)
  apply_macro_for_each_platform("SUPPORTED")
endmacro()

file(MAKE_DIRECTORY "${BUILTINS_OUTDIR_WITH_ARCH}/spirv")

set(OUTPUT_FILES_SPIRV_PREV)

# SPV addressing mode prefixes
set(SPV_STATEFUL_PREFIXES "bindful_image_bindful_buffer" "bindless_image_bindless_buffer")
set(SPV_STATELESS_PREFIXES "bindful_image_stateless_buffer" "bindless_image_stateless_buffer")
set(SPV_WIDE_PREFIXES "bindful_image_stateless_buffer_wide" "bindless_image_stateless_buffer_wide")

# Stateful SPVs
foreach(ADDR_MODE ${SPV_STATEFUL_PREFIXES})
  foreach(KERNEL ${BUILTINS_BUFFER} ${BUILTINS_IMAGE_BUFFER} ${BUILTINS_IMAGE} ${BUILTINS_AUX_TRANSLATION})
    generate_cpp_spirv(${KERNEL} "${ADDR_MODE}" "" FALSE)
  endforeach()
endforeach()

# Stateless SPVs
foreach(ADDR_MODE ${SPV_STATELESS_PREFIXES})
  foreach(KERNEL ${BUILTINS_BUFFER} ${BUILTINS_IMAGE_BUFFER} ${BUILTINS_IMAGE} ${BUILTINS_AUX_TRANSLATION})
    generate_cpp_spirv(${KERNEL} "${ADDR_MODE}" "${BUILTIN_OPTIONS_STATELESS}" FALSE)
  endforeach()
endforeach()

# Wide stateless SPVs
foreach(ADDR_MODE ${SPV_WIDE_PREFIXES})
  foreach(KERNEL ${BUILTINS_BUFFER} ${BUILTINS_IMAGE_BUFFER} ${BUILTINS_IMAGE})
    generate_cpp_spirv(${KERNEL} "${ADDR_MODE}" "${BUILTIN_OPTIONS_STATELESS}" TRUE)
  endforeach()
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
