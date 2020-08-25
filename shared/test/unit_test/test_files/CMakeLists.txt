#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

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

function(compile_kernels_gen platform_name_with_type platform_name suffix)

  set(outputdir "${TargetDir}/${suffix}/test_files/${NEO_ARCH}/")
  set(compiled_kernels)

  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)

    set(outputpath_base "${outputdir}${basename}_${suffix}")

    set(output_files
        ${outputpath_base}.spv
        ${outputpath_base}.bin
        ${outputpath_base}.gen
    )

    add_custom_command(
                       OUTPUT ${output_files}
                       COMMAND ${cloc_cmd_prefix} -file ${filename} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir}
                       WORKING_DIRECTORY ${workdir}
                       DEPENDS ${filepath} ocloc copy_compiler_files
    )

    list(APPEND compiled_kernels ${output_files})

  endforeach()
  list(APPEND compiled_kernels_${platform_name_with_type} ${compiled_kernels})
  set(compiled_kernels_${platform_name_with_type} ${compiled_kernels_${platform_name_with_type}} PARENT_SCOPE)
endfunction()

function(neo_shared_copy_test_files target product)
  string(TOLOWER ${product} product)
  set(dest_dir "${TargetDir}/${product}/test_files")
  set(source_dir "${NEO_SOURCE_DIR}/shared/test/unit_test/test_files")
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
  set_target_properties(${target} PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}")
endfunction()

file(GLOB_RECURSE TEST_KERNELS *.cl)
add_custom_target(prepare_test_kernel_for_shared)

macro(macro_for_each_platform)
  neo_shared_copy_test_files(copy_test_kernel_${PLATFORM_IT} ${PLATFORM_IT})
  add_dependencies(prepare_test_kernel_for_shared copy_test_kernel_${PLATFORM_IT})
endmacro()

macro(macro_for_each_gen)
  apply_macro_for_each_platform("TESTED")
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})

    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})

      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      set(PLATFORM_LOWER ${DEFAULT_SUPPORTED_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})

      compile_kernels_gen(${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${TEST_KERNELS})

      add_custom_target(prepare_test_kernel_for_shared_${family_name_with_type} DEPENDS ${compiled_kernels_${family_name_with_type}})
      set_target_properties(prepare_test_kernel_for_shared_${family_name_with_type} PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${family_name_with_type}")
      add_dependencies(prepare_test_kernel_for_shared prepare_test_kernel_for_shared_${family_name_with_type})

    endif()
  endforeach()
endmacro()

apply_macro_for_each_gen("TESTED")
