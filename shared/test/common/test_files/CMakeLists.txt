#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT NEO_SKIP_AUB_TESTS_RUN)
  function(neo_gen_kernels platform_it_lower device revision_id)
    set(outputdir "${TargetDir}/${platform_it_lower}/${revision_id}/test_files/${NEO_ARCH}/")

    if("${CORE_TYPE}" IN_LIST XE3P_AND_LATER_CORE_TYPES)
      set(addrmode "default")
    else()
      set(addrmode "bindful")
    endif()

    set(kernels_to_compile)
    foreach(filepath ${ARGN})
      get_filename_component(basename ${filepath} NAME_WE)
      get_filename_component(workdir ${filepath} DIRECTORY)
      get_filename_component(absolute_filepath ${filepath} ABSOLUTE)

      set(outputname_base "${basename}_${platform_it_lower}")
      set(outputpath_base "${outputdir}${outputname_base}")
      set(output_file "${outputpath_base}.bin")
      if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
        add_custom_command(
                           OUTPUT ${output_file}
                           COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -device ${device} -${NEO_BITS} -stateful_address_mode ${addrmode} -revision_id ${revision_id} -out_dir ${outputdir} -output_no_suffix -output ${outputname_base}
                           WORKING_DIRECTORY ${workdir}
                           DEPENDS ${filepath} ocloc copy_compiler_files ${PREVIOUS_KERNELS}
        )

        if(NEO_SERIALIZED_BUILTINS_COMPILATION)
          set(PREVIOUS_KERNELS ${output_file})
        endif()

        list(APPEND kernels_to_compile ${output_file})
      else()
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/${platform_it_lower}/${revision_id}/test_files/${NEO_ARCH}/${basename}_${platform_it_lower}.bin")
        add_custom_command(
                           OUTPUT ${output_file}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND kernels_to_compile_${platform_it_lower}_${revision_id} ${output_file})
      endif()
    endforeach()
    list(APPEND kernels_to_compile_${platform_it_lower}_${revision_id} ${kernels_to_compile})
    set(kernels_to_compile_${platform_it_lower}_${revision_id} ${kernels_to_compile_${platform_it_lower}_${revision_id}} PARENT_SCOPE)
  endfunction()

  add_custom_target(prepare_test_kernels_for_aub)
  set_target_properties(prepare_test_kernels_for_aub PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
  if(TARGET prepare_kernels)
    add_dependencies(prepare_kernels prepare_test_kernels_for_aub)
  endif()

  file(GLOB_RECURSE TEST_KERNELS *.cl)

  macro(macro_for_each_platform)
    set(PLATFORM_TEST_KERNELS ${TEST_KERNELS})

    foreach(KERNEL_TO_REMOVE ${${CORE_TYPE}_TEST_KERNELS_BLOCKLIST})
      set(KERNEL_TO_REMOVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${KERNEL_TO_REMOVE}")
      list(REMOVE_ITEM PLATFORM_TEST_KERNELS ${KERNEL_TO_REMOVE_PATH})
    endforeach()

    set(PREVIOUS_KERNELS)

    if(WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 8)
      foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
        parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
        neo_gen_kernels(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} ${PLATFORM_TEST_KERNELS})
      endforeach()

      #compile gen specific kernels if any were found
      file(GLOB_RECURSE ${CORE_TYPE_LOWER}_TEST_KERNELS *.${CORE_TYPE_LOWER})
      if(NOT "${${CORE_TYPE_LOWER}_TEST_KERNELS}" STREQUAL "")
        foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
          parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
          neo_gen_kernels(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} ${${CORE_TYPE_LOWER}_TEST_KERNELS})
        endforeach()
      endif()

      # Compile platform specific kernels if any were found
      file(GLOB_RECURSE ${PLATFORM_IT_LOWER}_TEST_KERNELS *.${PLATFORM_IT_LOWER})
      if(NOT "${${PLATFORM_IT_LOWER}_TEST_KERNELS}" STREQUAL "")
        foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
          parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
          neo_gen_kernels(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} ${${PLATFORM_IT_LOWER}_TEST_KERNELS})
        endforeach()
      endif()

    endif()

    foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
      parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
      add_custom_target(prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID} DEPENDS ${kernels_to_compile_${PLATFORM_IT_LOWER}_${REVISION_ID}} copy_compiler_files ${PREVIOUS_TARGET})
      add_dependencies(prepare_test_kernels_for_aub prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID})
      set_target_properties(prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID} PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/test platforms/${PLATFORM_IT_LOWER}/${REVISION_ID}")

      if(NEO_SERIALIZED_BUILTINS_COMPILATION)
        set(PREVIOUS_TARGET "prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID}")
      endif()

    endforeach()
  endmacro()

  macro(macro_for_each_core_type)
    apply_macro_for_each_platform("TESTED")
  endmacro()

  set(PREVIOUS_TARGET "")
  apply_macro_for_each_core_type("TESTED")
endif()
