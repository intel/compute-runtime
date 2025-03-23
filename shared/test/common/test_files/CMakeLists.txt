#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_TEST_PROJECTS_SUB_FOLDER "prepare test files")
function(compile_kernels_gen device revision_id platform_name use_stateless_suffix use_heapless)

  set(outputdir "${TargetDir}/${platform_name}/${revision_id}/test_files/${NEO_ARCH}/")
  set(compiled_kernels)

  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)
    get_filename_component(absolute_filepath ${filepath} ABSOLUTE)

    set(outputname_base "${basename}_${platform_name}")
    if(${use_heapless})
      set(outputname_base "${outputname_base}-heapless")
    elseif(${use_stateless_suffix})
      set(outputname_base "${outputname_base}-cl-intel-greater-than-4GB-buffer-required_")
    endif()

    set(outputpath_base "${outputdir}${outputname_base}")

    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(output_files
          ${outputpath_base}.spv
          ${outputpath_base}.bin
      )

      set(heapless_mode "disable")
      if(${use_heapless})
        set(heapless_mode "enable")
      endif()

      add_custom_command(
                         OUTPUT ${output_files}
                         COMMAND ${ocloc_cmd_prefix} -file ${absolute_filepath} -device ${device} -heapless_mode ${heapless_mode} -${NEO_BITS} -out_dir ${outputdir} -output_no_suffix -output ${outputname_base} -revision_id ${revision_id}
                         WORKING_DIRECTORY ${workdir}
                         DEPENDS ${filepath} ocloc copy_compiler_files
      )

      list(APPEND compiled_kernels_${platform_name}_${revision_id} ${output_files})
    else()
      foreach(file_extension "spv" "bin")
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/${platform_name}/${revision_id}/test_files/${NEO_ARCH}/${outputname_base}.${file_extension}")
        add_custom_command(
                           OUTPUT ${outputpath_base}.${file_extension}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND compiled_kernels_${platform_name}_${revision_id} ${outputpath_base}.${file_extension})
      endforeach()
    endif()
  endforeach()
  set(compiled_kernels_${platform_name}_${revision_id} ${compiled_kernels_${platform_name}_${revision_id}} PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE TEST_KERNELS *.cl)
file(GLOB_RECURSE TEST_KERNELS_IMAGES *_images.cl)
add_custom_target(prepare_test_kernels_for_shared)
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_STATELESS_LIB_NAME})
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_HEAPLESS_LIB_NAME})
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_BINDFUL_LIB_NAME})
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_BINDLESS_LIB_NAME})
if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
  add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_SPIRV_LIB_NAME})
endif()
set_target_properties(prepare_test_kernels_for_shared PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/${SHARED_TEST_PROJECTS_SUB_FOLDER}")
add_dependencies(prepare_test_kernels_for_shared copy_compiler_files)

macro(macro_for_each_platform)
  set(KERNELS_TO_COMPILE ${TEST_KERNELS})
  set(IMAGE_SUPPORT FALSE)
  CORE_CONTAINS_PLATFORM("SUPPORTED_IMAGES" ${CORE_TYPE} ${PLATFORM_IT} IMAGE_SUPPORT)
  if(NOT ${IMAGE_SUPPORT})
    list(REMOVE_ITEM KERNELS_TO_COMPILE ${TEST_KERNELS_IMAGES})
  endif()

  set(STATELESS_SUPPORT FALSE)
  CORE_CONTAINS_PLATFORM("SUPPORTED_STATELESS" ${CORE_TYPE} ${PLATFORM_IT} STATELESS_SUPPORT)

  foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
    parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)

    compile_kernels_gen(${DEVICE_ID} ${REVISION_ID} ${PLATFORM_IT_LOWER} FALSE FALSE ${KERNELS_TO_COMPILE})

    if(${STATELESS_SUPPORT})
      compile_kernels_gen(${DEVICE_ID} ${REVISION_ID} ${PLATFORM_IT_LOWER} TRUE FALSE ${KERNELS_TO_COMPILE})
    endif()

    if(${HEAPLESS_SUPPORT})
      compile_kernels_gen(${DEVICE_ID} ${REVISION_ID} ${PLATFORM_IT_LOWER} FALSE TRUE ${KERNELS_TO_COMPILE})
    endif()

    add_custom_target(prepare_test_kernels_for_shared_${PLATFORM_IT_LOWER}_${REVISION_ID} DEPENDS ${compiled_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID}} ${PREVIOUS_TARGET})
    set_target_properties(prepare_test_kernels_for_shared_${PLATFORM_IT_LOWER}_${REVISION_ID} PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/${SHARED_TEST_PROJECTS_SUB_FOLDER}/${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${PLATFORM_IT_LOWER}/${REVISION_ID}")
    add_dependencies(prepare_test_kernels_for_shared prepare_test_kernels_for_shared_${PLATFORM_IT_LOWER}_${REVISION_ID})

    if(NEO_SERIALIZED_BUILTINS_COMPILATION)
      set(PREVIOUS_TARGET "prepare_test_kernels_for_shared_${PLATFORM_IT_LOWER}_${REVISION_ID}")
    endif()
  endforeach()
endmacro()

macro(macro_for_each_core_type)
  apply_macro_for_each_platform("TESTED")
endmacro()

set(PREVIOUS_TARGET "")
apply_macro_for_each_core_type("TESTED")
