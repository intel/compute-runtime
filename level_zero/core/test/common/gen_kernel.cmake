#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

function(level_zero_generate_kernels target_list platform_name revision_id options)

  list(APPEND results copy_compiler_files)

  set(relativeDir "level_zero/${platform_name}/${revision_id}/test_files/${NEO_ARCH}")

  set(outputdir "${TargetDir}/${relativeDir}/")

  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)
    get_filename_component(absolute_filepath ${filepath} ABSOLUTE)

    set(outputpath_base "${outputdir}${basename}_${platform_name}")
    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(output_files
          ${outputpath_base}.bin
          ${outputpath_base}.spv
      )

      add_custom_command(
                         COMMAND echo generate ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -device ${platform_name} -out_dir ${outputdir} -revision_id ${revision_id} -options "${options}"
                         OUTPUT ${output_files}
                         COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -device ${platform_name} -out_dir ${outputdir} -revision_id ${revision_id} -options "${options}"
                         WORKING_DIRECTORY ${workdir}
                         DEPENDS ${filepath} ocloc
      )

      list(APPEND ${target_list} ${output_files})
    else()
      foreach(extension "bin" "spv")
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/${relativeDir}/${basename}_${platform_name}.${extension}")
        add_custom_command(
                           OUTPUT ${outputpath_base}.${extension}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND ${target_list} ${outputpath_base}.${extension})
      endforeach()
    endif()
  endforeach()

  set(${target_list} ${${target_list}} PARENT_SCOPE)
endfunction()

function(level_zero_generate_kernels_with_internal_options target_list platform_name prefix revision_id options internal_options)

  list(APPEND results copy_compiler_files)

  set(relativeDir "level_zero/${platform_name}/${revision_id}/test_files/${NEO_ARCH}")

  set(outputdir "${TargetDir}/${relativeDir}/")

  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)
    get_filename_component(absolute_filepath ${filepath} ABSOLUTE)

    set(outputpath_base "${outputdir}${prefix}_${basename}_${platform_name}")

    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(output_files
          ${outputpath_base}.bin
          ${outputpath_base}.spv
      )

      set(output_name "-output" "${prefix}_${basename}")

      add_custom_command(
                         COMMAND echo generate ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -device ${platform_name} -out_dir ${outputdir} ${output_name} -revision_id ${revision_id} -options ${options} -internal_options "$<JOIN:${internal_options}, >" , workdir is ${workdir}
                         OUTPUT ${output_files}
                         COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -device ${platform_name} -out_dir ${outputdir} ${output_name} -revision_id ${revision_id} -options ${options} -internal_options "$<JOIN:${internal_options}, >"
                         WORKING_DIRECTORY ${workdir}
                         DEPENDS ${filepath} ocloc
      )

      list(APPEND ${target_list} ${output_files})
    else()
      foreach(extension "bin" "spv")
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/${relativeDir}/${prefix}_${basename}_${platform_name}.${extension}")
        add_custom_command(
                           OUTPUT ${outputpath_base}.${extension}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND ${target_list} ${outputpath_base}.${extension})
      endforeach()
    endif()
  endforeach()

  set(${target_list} ${${target_list}} PARENT_SCOPE)
endfunction()
