#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_tests)

set(OPENCL_TEST_PROJECTS_FOLDER "opencl runtime")
set(PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER "${OPENCL_TEST_PROJECTS_FOLDER}/test platforms")
set(OPENCL_UNIT_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
  set(NEO_SUPPORTED_TEST_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
  string(REPLACE ";" "," NEO_SUPPORTED_TEST_PRODUCT_FAMILIES "${NEO_SUPPORTED_TEST_PRODUCT_FAMILIES}")
  add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${NEO_SUPPORTED_TEST_PRODUCT_FAMILIES})
endfunction()

ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()
link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

add_custom_target(prepare_test_kernels_for_ocl)
add_dependencies(prepare_test_kernels_for_ocl ${BUILTINS_BINARIES_BINDFUL_LIB_NAME})
add_custom_target(run_unit_tests ALL)

add_custom_target(run_ocl_tests)
set_target_properties(run_ocl_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})

set(IGDRCL_SRCS_tests_local
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${NEO_SHARED_TEST_DIRECTORY}/common/tests_configuration.h
)

add_subdirectory(libult)

hide_subdir(libult)
hide_subdir(linux)

if(UNIX)
  add_subdirectory(linux)
  add_custom_command(
                     TARGET run_ocl_tests
                     POST_BUILD
                     COMMAND echo running tests for linux dynamic library - .so in ${TargetDir}
                     COMMAND ${NEO_RUN_INTERCEPTOR_LIST} igdrcl_linux_dll_tests ${NEO_TESTS_LISTENER_OPTION}
                     COMMAND WORKING_DIRECTORY ${TargetDir}
  )
endif()

set(NEO_IGDRCL_TESTS__TARGET_OBJECTS
    $<TARGET_OBJECTS:neo_libult_common>
    $<TARGET_OBJECTS:igdrcl_libult>
    $<TARGET_OBJECTS:neo_libult_cs>
    $<TARGET_OBJECTS:neo_libult>
    $<TARGET_OBJECTS:igdrcl_libult_env>
    $<TARGET_OBJECTS:mock_aubstream>
    $<TARGET_OBJECTS:mock_gmm>
    $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
    $<TARGET_OBJECTS:neo_shared_mocks>
    $<TARGET_OBJECTS:neo_unit_tests_config>
)

add_executable(igdrcl_tests
               ${NEO_IGDRCL_TESTS__TARGET_OBJECTS}
               ${IGDRCL_SRCS_tests_local}
)

hide_subdir(gen_common)
add_subdirectory(gen_common)

if(NOT GTEST_EXCEPTION_OPTIONS)
  set(GTEST_EXCEPTION_OPTIONS --gtest_catch_exceptions=1)
endif()
message(STATUS "GTest exception options set to ${GTEST_EXCEPTION_OPTIONS}")

if(GTEST_FILTERING_PATTERN)
  set(GTEST_FILTER_OPTION "--gtest_filter=${GTEST_FILTERING_PATTERN}")
  message(STATUS "GTest filter for regular tests: ${GTEST_FILTERING_PATTERN}")
endif()

if(USE_ASAN)
  set(GTEST_ENV "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan_suppressions.txt")
endif()

target_link_libraries(igdrcl_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_tests igdrcl_mocks)

target_include_directories(igdrcl_tests PRIVATE
                           ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks${BRANCH_DIR_SUFFIX}
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
)
if(WIN32)
  target_include_directories(igdrcl_tests PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory${BRANCH_DIR_SUFFIX}
  )
endif()

target_link_libraries(igdrcl_tests gmock-gtest ${NEO_EXTRA_LIBS})

set(BUILT_IN_KERNEL_DIR "${NEO_SOURCE_DIR}/shared/source/built_ins")

add_dependencies(unit_tests
                 igdrcl_tests
                 prepare_test_kernels_for_ocl
                 prepare_test_kernels_for_shared
)

set_target_properties(igdrcl_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
if(UNIX)
  set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()
set_property(TARGET igdrcl_tests PROPERTY ENABLE_EXPORTS TRUE)

set_target_properties(unit_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_target_properties(prepare_test_kernels_for_ocl PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_target_properties(run_unit_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})

target_include_directories(igdrcl_tests BEFORE PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/includes${BRANCH_DIR_SUFFIX}
)

function(neo_gen_kernels platform_it_lower device revision_id forcePatchtokenFormat)
  set(outputdir "${TargetDir}/${platform_it_lower}/${revision_id}/test_files/${NEO_ARCH}/")

  if(forcePatchtokenFormat)
    set(formatArgument "--format" "patchtokens")
  endif()
  set(addrmode "bindful")

  set(kernels_to_compile)
  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)
    get_filename_component(workdir ${filepath} DIRECTORY)
    get_filename_component(absolute_filepath ${filepath} ABSOLUTE)

    set(outputname_base "${basename}_${platform_it_lower}")
    set(outputpath_base "${outputdir}${outputname_base}")
    if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
      set(output_files
          ${outputpath_base}.spv
          ${outputpath_base}.bin
      )

      add_custom_command(
                         OUTPUT ${output_files}
                         COMMAND ${ocloc_cmd_prefix} -q -file ${absolute_filepath} -device ${device} -${NEO_BITS} -stateful_address_mode ${addrmode} -revision_id ${revision_id} -out_dir ${outputdir} ${formatArgument} -output_no_suffix -output ${outputname_base}
                         WORKING_DIRECTORY ${workdir}
                         DEPENDS ${filepath} ocloc ${PREVIOUS_KERNELS}
      )

      if(NEO_SERIALIZED_BUILTINS_COMPILATION)
        set(PREVIOUS_KERNELS ${output_files})
      endif()

      list(APPEND kernels_to_compile ${output_files})
    else()
      foreach(extension "spv" "bin")
        set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/${platform_it_lower}/${revision_id}/test_files/${NEO_ARCH}/${basename}_${platform_it_lower}.${extension}")
        add_custom_command(
                           OUTPUT ${outputpath_base}.${extension}
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
        )

        list(APPEND kernels_to_compile_${platform_it_lower}_${revision_id} ${outputpath_base}.${extension})
      endforeach()
    endif()
  endforeach()
  list(APPEND kernels_to_compile_${platform_it_lower}_${revision_id} ${kernels_to_compile})
  set(kernels_to_compile_${platform_it_lower}_${revision_id} ${kernels_to_compile_${platform_it_lower}_${revision_id}} PARENT_SCOPE)
endfunction()

function(neo_gen_kernels_with_options platform_it_lower device revision_id filepath)
  set(kernels_to_compile)
  foreach(filearg ${filepath})
    get_filename_component(filename ${filearg} NAME)
    get_filename_component(basename ${filearg} NAME_WE)
    get_filename_component(base_workdir ${filearg} DIRECTORY)
    get_filename_component(absolute_filepath ${filearg} ABSOLUTE)

    set(outputdir "${TargetDir}/${platform_it_lower}/${revision_id}/test_files/${NEO_ARCH}/")
    set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/${base_workdir}/")
    set(addrmode "bindful")

    foreach(arg ${ARGN})
      string(REPLACE " " "_" argwospaces ${arg})
      set(base_filename ${basename}_${argwospaces})
      set(outputname_base "${base_filename}_${platform_it_lower}")
      set(outputpath_base "${outputdir}/${outputname_base}")
      if(NOT NEO_DISABLE_BUILTINS_COMPILATION)
        set(output_files
            ${outputpath_base}.spv
            ${outputpath_base}.bin
        )

        add_custom_command(
                           OUTPUT ${output_files}
                           COMMAND ${ocloc_cmd_prefix} -file ${absolute_filepath} -device ${device} -${NEO_BITS} -stateful_address_mode ${addrmode} -out_dir ${outputdir} -output_no_suffix -output ${outputname_base} -revision_id ${revision_id} -options ${arg}
                           WORKING_DIRECTORY ${workdir}
                           DEPENDS ${filearg} ocloc ${PREVIOUS_KERNELS}
        )

        if(NEO_SERIALIZED_BUILTINS_COMPILATION)
          set(PREVIOUS_KERNELS ${output_files})
        endif()
        list(APPEND kernels_to_compile ${output_files})
      else()
        foreach(extension "spv" "bin")
          set(_file_prebuilt "${NEO_KERNELS_BIN_DIR}/${platform_it_lower}/${revision_id}/test_files/${NEO_ARCH}/${base_filename}_${platform_it_lower}.${extension}")
          add_custom_command(
                             OUTPUT ${outputpath_base}.${extension}
                             COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}
                             COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file_prebuilt} ${outputdir}
          )

          list(APPEND kernels_to_compile_${platform_it_lower}_${revision_id} ${outputpath_base}.${extension})
        endforeach()
      endif()
    endforeach()
  endforeach()
  list(APPEND kernels_to_compile_${platform_it_lower}_${revision_id} ${kernels_to_compile})
  set(kernels_to_compile_${platform_it_lower}_${revision_id} ${kernels_to_compile_${platform_it_lower}_${revision_id}} PARENT_SCOPE)
endfunction()

set(TEST_KERNEL test_files/CopyBuffer_simd16.cl)

set(TEST_KERNEL_options
    "-cl-fast-relaxed-math"
    "-cl-finite-math-only"
    "-cl-kernel-arg-info"
    "-x spir -spir-std=1.2"
)

file(GLOB_RECURSE TEST_KERNELS test_files/*.cl)

macro(macro_for_each_platform)
  set(PLATFORM_TEST_KERNELS ${TEST_KERNELS})
  set(IMAGE_SUPPORT FALSE)
  CORE_CONTAINS_PLATFORM("SUPPORTED_IMAGES" ${CORE_TYPE} ${PLATFORM_IT} IMAGE_SUPPORT)

  foreach(KERNEL_TO_REMOVE ${${CORE_TYPE}_TEST_KERNELS_BLOCKLIST})
    set(KERNEL_TO_REMOVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/test_files/${KERNEL_TO_REMOVE}")
    list(REMOVE_ITEM PLATFORM_TEST_KERNELS ${KERNEL_TO_REMOVE_PATH})
  endforeach()

  set(PREVIOUS_KERNELS)

  if(WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 8)
    foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
      parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
      neo_gen_kernels(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} FALSE ${PLATFORM_TEST_KERNELS})
      neo_gen_kernels_with_options(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} ${TEST_KERNEL} ${TEST_KERNEL_options})
    endforeach()

    #compile gen specific kernels if any were found
    file(GLOB_RECURSE ${CORE_TYPE_LOWER}_TEST_KERNELS test_files/*.${CORE_TYPE_LOWER})
    if(NOT "${${CORE_TYPE_LOWER}_TEST_KERNELS}" STREQUAL "")
      foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
        parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
        neo_gen_kernels(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} FALSE ${${CORE_TYPE_LOWER}_TEST_KERNELS})
      endforeach()
    endif()

    # Compile platform specific kernels if any were found
    file(GLOB_RECURSE ${PLATFORM_IT_LOWER}_TEST_KERNELS test_files/*.${PLATFORM_IT_LOWER})
    if(NOT "${${PLATFORM_IT_LOWER}_TEST_KERNELS}" STREQUAL "")
      foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
        parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
        neo_gen_kernels(${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} FALSE ${${PLATFORM_IT_LOWER}_TEST_KERNELS})
      endforeach()
    endif()

  endif()

  foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
    parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
    add_custom_target(prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID} DEPENDS ${kernels_to_compile_${PLATFORM_IT_LOWER}_${REVISION_ID}} copy_compiler_files ${PREVIOUS_TARGET})
    add_dependencies(prepare_test_kernels_for_ocl prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID})
    set_target_properties(prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID} PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${PLATFORM_IT_LOWER}/${REVISION_ID}")

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
add_subdirectories()
create_project_source_tree(igdrcl_tests)

set(UltPchHeader "${CMAKE_CURRENT_SOURCE_DIR}/igdrcl_tests_pch.h")
set(UltPchSource "${CMAKE_CURRENT_SOURCE_DIR}/igdrcl_tests_pch.cpp")
get_target_property(UltSources igdrcl_tests SOURCES)

if(MSVC AND NOT DISABLE_ULT_PCH_WIN)
  set(UltPchBinary "${CMAKE_CURRENT_BINARY_DIR}/igdrcl_tests_pch.pch")

  set(IGDRCL_SRCS_ult_pch ${UltPchSource} ${UltPchHeader})
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_ult_pch})

  set_source_files_properties(${UltSources}
                              PROPERTIES COMPILE_FLAGS "/Yu${UltPchHeader} /FI${UltPchHeader} /Fp${UltPchBinary}"
                              OBJECT_DEPENDS "${UltPchBinary}"
  )

  set_source_files_properties(${UltPchSource}
                              PROPERTIES COMPILE_FLAGS "/Yc${UltPchHeader} /FI${UltPchHeader} /Fp${UltPchBinary}"
                              OBJECT_OUTPUTS "${UltPchBinary}"
  )
elseif(USE_ULT_PCH)
  set(UltPchHeaderInBuildDir "${CMAKE_CURRENT_BINARY_DIR}/igdrcl_tests_pch.h")
  set(UltPchBinaryGch "${UltPchHeaderInBuildDir}.gch")
  set(UltPchBinary "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/igdrcl_ult_pch.dir/igdrcl_tests_pch.h.o")

  add_library(igdrcl_ult_pch STATIC EXCLUDE_FROM_ALL ${UltPchHeader})
  add_dependencies(igdrcl_tests igdrcl_ult_pch)

  target_include_directories(igdrcl_ult_pch PRIVATE
                             $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                             $<TARGET_PROPERTY:igdrcl_tests,INCLUDE_DIRECTORIES>
  )
  target_compile_definitions(igdrcl_ult_pch PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

  target_include_directories(igdrcl_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  if(NOT USE_ASAN)
    set_source_files_properties(${UltSources}
                                PROPERTIES COMPILE_FLAGS "-include ${UltPchHeaderInBuildDir} -msse4"
                                OBJECT_DEPENDS ${UltPchBinaryGch}
    )
  endif()

  set_source_files_properties(${UltPchHeader}
                              PROPERTIES LANGUAGE "CXX"
                              COMPILE_FLAGS "-x c++-header -msse4 -gdwarf-2"
  )

  add_custom_command(
                     OUTPUT ${UltPchBinaryGch}
                     COMMAND cp "${UltPchHeader}" "${UltPchHeaderInBuildDir}"
                     COMMAND cp "${UltPchBinary}" "${UltPchBinaryGch}"
                     DEPENDS ${UltPchBinary}
  )
endif()
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!                                                                             !!
# !!              DONT ADD ANY SOURCES HERE!                                     !!
# !!                                                                             !!
# !!      You are below PCH logic!                                               !!
# !!      This is to keep PCH dependencies correctly without creating new target !!
# !!                                                                             !!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
