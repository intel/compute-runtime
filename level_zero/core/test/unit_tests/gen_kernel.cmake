#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

function(level_zero_gen_kernels target platform_name suffix options)

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

  set(outputdir "${TargetDir}/level_zero/${suffix}/test_files/${NEO_ARCH}/")

  set(results)
  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)

    set(outputpath_base "${outputdir}${basename}_${suffix}")
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

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER ${TARGET_NAME_L0})
endfunction()
