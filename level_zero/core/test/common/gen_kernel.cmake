#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

function(level_zero_gen_kernels target_list platform_name suffix options)

  if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
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
    list(APPEND results copy_compiler_files)
  endif()

  set(outputdir "${TargetDir}/level_zero/${suffix}/test_files/${NEO_ARCH}/")

  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)

    set(outputpath_base "${outputdir}${basename}_${suffix}")
    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(output_files
          ${outputpath_base}.bin
          ${outputpath_base}.gen
      )

      add_custom_command(
                         COMMAND echo generate ${cloc_cmd_prefix} -q -file ${filename} -device ${platform_name} -out_dir ${outputdir} -options "${options}"
                         OUTPUT ${output_files}
                         COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${platform_name} -out_dir ${outputdir} -options "${options}"
                         WORKING_DIRECTORY ${workdir}
                         DEPENDS ${filepath} ocloc
      )

      list(APPEND ${target_list} ${output_files})
    else()
      foreach(_file_name "bin" "gen")
        set(_file_prebuilt "${NEO_SOURCE_DIR}/../neo_test_kernels/level_zero/${suffix}/test_files/${NEO_ARCH}/${basename}_${suffix}.${_file_name}")
        add_custom_command(
                           OUTPUT ${outputpath_base}.${_file_name}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND ${target_list} ${outputpath_base}.${_file_name})
      endforeach()
    endif()
  endforeach()

  set(${target_list} ${${target_list}} PARENT_SCOPE)
endfunction()
