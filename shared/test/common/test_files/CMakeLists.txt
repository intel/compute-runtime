#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_TEST_PROJECTS_SUB_FOLDER "prepare test files")
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

function(compile_kernels_gen platform_name_with_type revision_id platform_name suffix)

  set(outputdir "${TargetDir}/${suffix}/${revision_id}/test_files/${NEO_ARCH}/")
  set(compiled_kernels)

  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)

    set(outputpath_base "${outputdir}${basename}_${suffix}")

    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(output_files
          ${outputpath_base}.spv
          ${outputpath_base}.bin
          ${outputpath_base}.gen
      )

      add_custom_command(
                         OUTPUT ${output_files}
                         COMMAND ${cloc_cmd_prefix} -file ${filename} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir} -revision_id ${revision_id}
                         WORKING_DIRECTORY ${workdir}
                         DEPENDS ${filepath} ocloc copy_compiler_files
      )

      list(APPEND compiled_kernels_${platform_name_with_type}_${revision_id} ${output_files})
    else()
      foreach(_file_name "spv" "bin" "gen")
        set(_file_prebuilt "${NEO_SOURCE_DIR}/../neo_test_kernels/${suffix}/${revision_id}/test_files/${NEO_ARCH}/${basename}_${suffix}.${_file_name}")
        add_custom_command(
                           OUTPUT ${outputpath_base}.${_file_name}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND compiled_kernels_${platform_name_with_type}_${revision_id} ${outputpath_base}.${_file_name})
      endforeach()
    endif()
  endforeach()
  set(compiled_kernels_${platform_name_with_type}_${revision_id} ${compiled_kernels_${platform_name_with_type}_${revision_id}} PARENT_SCOPE)
endfunction()

function(neo_shared_copy_test_files target product revision_id)
  string(TOLOWER ${product} product)
  set(dest_dir "${TargetDir}/${product}/${revision_id}/test_files")
  set(source_dir "${NEO_SOURCE_DIR}/shared/test/common/test_files")
  add_custom_target(${target})
  add_custom_command(
                     TARGET ${target}
                     POST_BUILD
                     COMMAND echo copying test files from ${source_dir} to ${dest_dir}
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${source_dir} ${dest_dir}
                     WORKING_DIRECTORY ${TargetDir}
                     DEPENDS ${source_dir}
  )
  add_dependencies(${target} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/${SHARED_TEST_PROJECTS_SUB_FOLDER}/${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")
endfunction()

file(GLOB_RECURSE TEST_KERNELS *.cl)
file(GLOB_RECURSE TEST_KERNELS_IMAGES *_images.cl)
add_custom_target(prepare_test_kernels_for_shared)
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_BINDFUL_LIB_NAME})
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_BINDLESS_LIB_NAME})
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_SPIRV_LIB_NAME})
set_target_properties(prepare_test_kernels_for_shared PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/${SHARED_TEST_PROJECTS_SUB_FOLDER}")

macro(macro_for_each_platform)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    foreach(REVISION_ID ${${PLATFORM_TYPE}_${GEN_TYPE}_REVISIONS})
      if(${PLATFORM_IT}_IS_${PLATFORM_TYPE})
        neo_shared_copy_test_files(copy_test_kernel_${PLATFORM_IT_LOWER}_${REVISION_ID} ${PLATFORM_IT} ${REVISION_ID})
        add_dependencies(prepare_test_kernels_for_shared copy_test_kernel_${PLATFORM_IT_LOWER}_${REVISION_ID})
      endif()
    endforeach()
  endforeach()
endmacro()

macro(macro_for_each_gen)
  apply_macro_for_each_platform("TESTED")
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})

    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})

      set(KERNELS_TO_COMPILE ${TEST_KERNELS})
      set(IMAGE_SUPPORT FALSE)
      GEN_CONTAINS_PLATFORMS("SUPPORTED_IMAGES" ${GEN_TYPE} IMAGE_SUPPORT)
      if(NOT IMAGE_SUPPORT)
        list(REMOVE_ITEM KERNELS_TO_COMPILE ${TEST_KERNELS_IMAGES})
      endif()

      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      set(PLATFORM_LOWER ${DEFAULT_SUPPORTED_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})
      foreach(REVISION_ID ${${PLATFORM_TYPE}_${GEN_TYPE}_REVISIONS})
        compile_kernels_gen(${family_name_with_type} ${REVISION_ID} ${PLATFORM_LOWER} ${family_name_with_type} ${KERNELS_TO_COMPILE})

        add_custom_target(prepare_test_kernels_for_shared_${family_name_with_type}_${REVISION_ID} DEPENDS ${compiled_kernels_${family_name_with_type}_${REVISION_ID}})
        set_target_properties(prepare_test_kernels_for_shared_${family_name_with_type}_${REVISION_ID} PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/${SHARED_TEST_PROJECTS_SUB_FOLDER}/${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${family_name_with_type}/${REVISION_ID}")
        add_dependencies(prepare_test_kernels_for_shared prepare_test_kernels_for_shared_${family_name_with_type}_${REVISION_ID})
      endforeach()
    endif()
  endforeach()
endmacro()

apply_macro_for_each_gen("TESTED")
