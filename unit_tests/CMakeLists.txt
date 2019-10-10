#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_tests)

set(TEST_PROJECTS_FOLDER "opencl runtime")
set(PLATFORM_SPECIFIC_TARGETS_FOLDER "${TEST_PROJECTS_FOLDER}/test platforms")

# disable optimizations for ults
if(UNIX)
  string(REPLACE "-O2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  #disable fortify source as this requires optimization to be on
  string(REPLACE "-D_FORTIFY_SOURCE=2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O0")
  set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} -O0")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O0")
  set(CMAKE_C_FLAGS_RELEASEINTERNAL "${CMAKE_C_FLAGS_RELEASEINTERNAL} -O0")
endif()

if(WIN32)
  string(REPLACE "/O2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
  string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
  string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELEASEINTERNAL ${CMAKE_CXX_FLAGS_RELEASEINTERNAL})
  string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELEASEINTERNAL ${CMAKE_C_FLAGS_RELEASEINTERNAL})
endif()

link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

add_custom_target(unit_tests)
add_custom_target(run_unit_tests ALL)

set(IGDRCL_SRCS_tests_local
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/libult/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_configuration.cpp
)

if(WIN32)
   list(APPEND IGDRCL_SRCS_tests_local ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp)
endif()

add_subdirectory(libult)

hide_subdir(libult)
hide_subdir(linux)

if(UNIX)
  add_subdirectory(linux)
  add_custom_command(
    TARGET run_unit_tests
    POST_BUILD
    COMMAND echo running linux specific tests for static library in ${TargetDir}
    COMMAND igdrcl_linux_tests ${IGDRCL_TESTS_LISTENER_OPTION}
    COMMAND echo running tests for linux dynamic library - .so in ${TargetDir}
    COMMAND igdrcl_linux_dll_tests ${IGDRCL_TESTS_LISTENER_OPTION}
    COMMAND WORKING_DIRECTORY ${TargetDir}
  )
  if(PRE_ULT_COMMAND)
    add_custom_command(
      TARGET unit_tests
      POST_BUILD
      COMMAND echo running ${PRE_ULT_COMMAND} utility before ULTs execution
      COMMAND ${PRE_ULT_COMMAND}
    )
  endif()
endif()

set(NEO_IGDRCL_TESTS__TARGET_OBJECTS
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
  $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
)

if(DEFINED AUB_STREAM_DIR)
  add_library (aub_stream_mock_lib OBJECT EXCLUDE_FROM_ALL
    ${NEO_SOURCE_DIR}/unit_tests/aub_stream_mocks/aub_stream_interface_mock.cpp
  )
  list(APPEND NEO_IGDRCL_TESTS__TARGET_OBJECTS
    $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>
    $<TARGET_OBJECTS:aub_stream_mock_lib>
  )
else()
   list(APPEND IGDRCL_SRCS_tests_local ${NEO_SOURCE_DIR}/runtime/aub/aub_stream_interface.cpp)
endif()

add_executable(igdrcl_tests
  ${NEO_IGDRCL_TESTS__TARGET_OBJECTS}
  ${IGDRCL_SRCS_tests_local}
)

hide_subdir(gen_common)
add_subdirectory(gen_common)

if(NOT GTEST_REPEAT)
  set(GTEST_REPEAT 1 CACHE STRING "Google test iterations")
endif()
set(GTEST_REPEAT ${GTEST_REPEAT} CACHE STRING "Google test iterations")

if(NOT DEFINED GTEST_XML_OUTPUT_DIR)
  set(GTEST_XML_OUTPUT_DIR "test_logs")
endif()
set(GTEST_XML_OUTPUT_DIR ${GTEST_XML_OUTPUT_DIR} CACHE STRING "Google Test xml output file location")

message(STATUS "GTest repeat count set to ${GTEST_REPEAT}")
if(NOT GTEST_SHUFFLE)
  set(GTEST_SHUFFLE --gtest_shuffle --gtest_random_seed=0)
endif()
message(STATUS "GTest shuffle set to ${GTEST_SHUFFLE}")

if(NOT GTEST_EXCEPTION_OPTIONS)
  set(GTEST_EXCEPTION_OPTIONS --gtest_catch_exceptions=1)
endif()
message(STATUS "GTest exception options set to ${GTEST_EXCEPTION_OPTIONS}")

if(GTEST_FILTERING_PATTERN)
  set(GTEST_FILTER_OPTION "--gtest_filter=${GTEST_FILTERING_PATTERN}")
  message(STATUS "GTest filter for regular tests: ${GTEST_FILTERING_PATTERN}")
endif()

if(NOT MSVC)
  set_source_files_properties(helpers/uint16_sse4_tests.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif()

target_link_libraries(igdrcl_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_tests igdrcl_mocks)

option(SHOW_VERBOSE_UTESTS_RESULTS "Use the default/verbose test output" OFF)

if(NOT SHOW_VERBOSE_UTESTS_RESULTS)
  set(IGDRCL_TESTS_LISTENER_OPTION "--disable_default_listener")
else()
  set(IGDRCL_TESTS_LISTENER_OPTION "--enable_default_listener")
endif()

target_include_directories(igdrcl_tests PRIVATE
  ${NEO_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
  ${ENGINE_NODE_DIR}
  ${KHRONOS_GL_HEADERS_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(igdrcl_tests gmock-gtest ${IGDRCL_EXTRA_LIBS})

set(BUILT_IN_KERNEL_DIR "${NEO_SOURCE_DIR}/runtime/built_ins")

function(neo_copy_test_files target product)
  set(outputdir "${TargetDir}/${product}")
  add_custom_target(${target})
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND echo deleting and re-creating ${product} cache directory...
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${outputdir}/cl_cache
    COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}/cl_cache
    COMMAND echo copying built-in kernel files from ${BUILT_IN_KERNEL_DIR}/kernels to ${outputdir}/test_files
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BUILT_IN_KERNEL_DIR}/kernels ${outputdir}/test_files
    COMMAND echo copying test files from ${NEO_SOURCE_DIR}/unit_tests/test_files to ${outputdir}/test_files
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEO_SOURCE_DIR}/unit_tests/test_files ${outputdir}/test_files
    COMMAND WORKING_DIRECTORY ${TargetDir}
    DEPENDS ${NEO_SOURCE_DIR}/unit_tests/test_files
  )
  add_dependencies(${target} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${product}")
endfunction()

add_dependencies(unit_tests
  igdrcl_tests
  test_dynamic_lib
)

set_target_properties(igdrcl_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
if(UNIX)
  set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()

set_target_properties(unit_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
set_target_properties(run_unit_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

target_include_directories(igdrcl_tests BEFORE PRIVATE ${NEO_SOURCE_DIR}/core/unit_tests/test_macros${BRANCH_DIR_SUFFIX})

if(WIN32)
  set(cloc_cmd_prefix ocloc)
else()
  if(DEFINED IGDRCL__IGC_LIBRARY_PATH)
    set(cloc_cmd_prefix LD_LIBRARY_PATH=${IGDRCL__IGC_LIBRARY_PATH} $<TARGET_FILE:ocloc>)
  else()
    set(cloc_cmd_prefix LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ocloc> $<TARGET_FILE:ocloc>)
  endif()
endif()

function(neo_gen_kernels target platform_name suffix)
  set(outputdir "${TargetDir}/${suffix}/test_files/${NEO_ARCH}/")

  set(results)
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
      COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir}
      WORKING_DIRECTORY ${workdir}
      DEPENDS ${filepath} ocloc
    )

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${suffix}")
endfunction()

function(neo_gen_kernels_with_options target platform_name suffix filepath)
  set(results)
  foreach(filearg ${filepath})
    get_filename_component(filename ${filearg} NAME)
    get_filename_component(basename ${filearg} NAME_WE)
    get_filename_component(base_workdir ${filearg} DIRECTORY)

    set(outputdir "${TargetDir}/${suffix}/test_files/${NEO_ARCH}/")
    set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/${base_workdir}/")

    foreach(arg ${ARGN})
      string(REPLACE " " "_" argwospaces ${arg})

      set(outputpath_base "${outputdir}/${basename}_${suffix}")
      set(output_files
        ${outputpath_base}.spv${argwospaces}
        ${outputpath_base}.bin${argwospaces}
        ${outputpath_base}.gen${argwospaces}
      )

      add_custom_command(
        OUTPUT ${output_files}
        COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir} -options ${arg} -options_name
        WORKING_DIRECTORY ${workdir}
        DEPENDS ${filearg} ocloc
      )

      list(APPEND results ${output_files})
    endforeach()
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${suffix}")
endfunction()

function(neo_gen_kernels_with_internal_options target platform_name suffix filepath)
  set(results)
  foreach(filearg ${filepath})
    get_filename_component(filename ${filearg} NAME)
    get_filename_component(basename ${filearg} NAME_WE)
    get_filename_component(base_workdir ${filearg} DIRECTORY)

    set(outputdir "${TargetDir}/${suffix}/test_files/${NEO_ARCH}/")
    set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/${base_workdir}/")

    foreach(arg ${ARGN})

      set(outputpath_base "${outputdir}/${basename}_${suffix}")
      set(output_files
        ${outputpath_base}.spv
        ${outputpath_base}.bin
        ${outputpath_base}.gen
      )

      add_custom_command(
        OUTPUT ${output_files}
        COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir} -internal_options ${arg}
        WORKING_DIRECTORY ${workdir}
        DEPENDS ${filearg} ocloc
      )

      list(APPEND results ${output_files})
    endforeach()
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${suffix}")
endfunction()

set(TEST_KERNEL_kernel_debug_enable
  "-cl-kernel-debug-enable"
)

function(neo_gen_kernel_with_kernel_debug_options target platform_name suffix filepath)
  get_filename_component(filename ${filepath} NAME)
  get_filename_component(basename ${filepath} NAME_WE)
  get_filename_component(base_workdir ${filepath} DIRECTORY)

  set(outputdir "${TargetDir}/${suffix}/test_files/${NEO_ARCH}/")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/${base_workdir}/")

  set(results)
  string(REPLACE " " "_" argwospaces ${TEST_KERNEL_kernel_debug_enable})

  set(outputpath_base "${outputdir}/${argwospaces}_${suffix}")
  set(output_files
    ${outputpath_base}.spv
    ${outputpath_base}.bin
    ${outputpath_base}.gen
    ${outputpath_base}.dbg
  )

  add_custom_command(
    OUTPUT ${output_files}
    COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir} -output ${argwospaces} -internal_options ${TEST_KERNEL_kernel_debug_enable} -options "-g"
    WORKING_DIRECTORY ${workdir}
    DEPENDS ${filepath} ocloc
  )

  list(APPEND results ${output_files})
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${suffix}")
endfunction()

function(neo_gen_kernel_from_ll target platform_name suffix filepath output_name compile_options)
  get_filename_component(filename ${filepath} NAME)
  get_filename_component(basename ${filepath} NAME_WE)

  set(outputdir "${TargetDir}/${suffix}/test_files/${NEO_ARCH}")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  set(outputpath_base "${outputdir}/${output_name}_${suffix}")
  set(output_files
    ${outputpath_base}.bin
    ${outputpath_base}.gen
  )
  string(CONCAT compile_options \" ${compile_options} \" )

  add_custom_command(
    OUTPUT ${output_files}
    COMMAND ${cloc_cmd_prefix} -q -file ${filename} -output ${output_name} -device ${platform_name} -${NEO_BITS} -out_dir ${outputdir} -internal_options ${compile_options} -llvm_input
    WORKING_DIRECTORY ${workdir}
    DEPENDS ${filepath} ocloc
  )

  list(APPEND results ${output_files})

  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${suffix}")
endfunction()

set(TEST_KERNEL test_files/CopyBuffer_simd8.cl)

set(TEST_KERNEL_options
  "-cl-fast-relaxed-math"
  "-cl-finite-math-only"
  "-cl-kernel-arg-info"
  "-x spir -spir-std=1.2"
)

set(TEST_KERNEL_2_0_options
  "-cl-std=CL2.0"
)

set(TEST_KERNEL_2_0
  test_files/simple_block_kernel.cl
  test_files/simple_nonuniform.cl
)

set(TEST_KERNEL_STATELESS_internal_options
  "-cl-intel-greater-than-4GB-buffer-required"
)

set(TEST_KERNEL_STATELESS_internal_options_gen9lp
  "-cl-intel-greater-than-4GB-buffer-required -m32"
)


set(TEST_KERNEL_STATELESS
  test_files/stateless_kernel.cl
)

set(TEST_KERNEL_VME
  ${CMAKE_CURRENT_SOURCE_DIR}/test_files/vme_kernels.cl
  ${CMAKE_CURRENT_SOURCE_DIR}/test_files/media_kernels_backend.cl
  ${CMAKE_CURRENT_SOURCE_DIR}/test_files/media_kernels_frontend.cl
)

set(TEST_KERNEL_SIP_DEBUG_options
  "-cl-include-sip-kernel-debug -cl-include-sip-csr -cl-set-bti:0"
)

set(TEST_KERNEL_SIP_DEBUG_LOCAL_options
  "-cl-include-sip-kernel-local-debug -cl-include-sip-csr -cl-set-bti:0"
)

file(GLOB_RECURSE TEST_KERNELS test_files/*.cl)
list(REMOVE_ITEM TEST_KERNELS "${CMAKE_CURRENT_SOURCE_DIR}/test_files/shouldfail.cl")
list(REMOVE_ITEM TEST_KERNELS "${CMAKE_CURRENT_SOURCE_DIR}/test_files/simple_block_kernel.cl")
list(REMOVE_ITEM TEST_KERNELS "${CMAKE_CURRENT_SOURCE_DIR}/test_files/simple_nonuniform.cl")
list(REMOVE_ITEM TEST_KERNELS "${CMAKE_CURRENT_SOURCE_DIR}/test_files/stateless_kernel.cl")
list(REMOVE_ITEM TEST_KERNELS ${TEST_KERNEL_VME}
)

function(gen_run_tests_with_appverifier product slices subslices eu_per_ss)
  if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND "${IGDRCL_OPTION__BITS}" STREQUAL "64" AND APPVERIFIER_ALLOWED)
    add_custom_command(
      TARGET run_${product}_unit_tests
      POST_BUILD
      COMMAND echo copying test verify.bat file from ${CMAKE_CURRENT_SOURCE_DIR} to ${TargetDir}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC} ${DST} ${CMAKE_CURRENT_SOURCE_DIR}/verify.bat ${TargetDir}/verify.bat
      COMMAND WORKING_DIRECTORY ${TargetDir}
      COMMAND echo Running igdrcl_tests with App Verifier
      COMMAND ${TargetDir}/verify.bat --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
      COMMAND echo App Verifier returned: %errorLevel%
    )
  endif()
endfunction()

function(gen_run_unit_tests product slices subslices eu_per_ss)
  add_custom_target(run_${product}_unit_tests ALL DEPENDS unit_tests)
  neo_copy_test_files(copy_test_files_${product} ${product})
  add_dependencies(unit_tests copy_test_files_${product})
  set_target_properties(run_${product}_unit_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${product}")
  add_custom_command(
    TARGET run_${product}_unit_tests
    POST_BUILD
    COMMAND WORKING_DIRECTORY ${TargetDir}
    COMMAND echo Running igdrcl_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
    COMMAND ${CMAKE_COMMAND} -E env GTEST_OUTPUT=xml:${GTEST_XML_OUTPUT_DIR}/test_details_${product}_${slices}_${subslices}_${eu_per_ss}.xml $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
  )
  add_dependencies(run_unit_tests run_${product}_unit_tests)
endfunction()

macro(macro_for_each_test_config)
  gen_run_unit_tests(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
  if(MSVC AND ${TESTED_WITH_APPVERIFIER})
    gen_run_tests_with_appverifier(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
  endif()
endmacro()

macro(macro_for_each_platform)
  PLATFORM_TESTED_WITH_APPVERIFIER(${GEN_TYPE} ${PLATFORM_IT} TESTED_WITH_APPVERIFIER)
  apply_macro_for_each_test_config("UNIT_TESTS")
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${PLATFORM_IT}_IS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      add_dependencies(unit_tests test_kernels_${family_name_with_type})
      add_dependencies(unit_tests test_kernel_${family_name_with_type})
      add_dependencies(unit_tests test_kernel_stateless_${family_name_with_type})
      if(NOT ("${GEN_TYPE_LOWER}" STREQUAL "gen8"))
        add_dependencies(unit_tests test_kernel_debug_enable_${family_name_with_type})
      endif()
      PLATFORM_HAS_2_0(${GEN_TYPE} ${PLATFORM_IT} PLATFORM_SUPPORTS_2_0)
      if(PLATFORM_SUPPORTS_2_0)
        add_dependencies(unit_tests test_kernel_2_0_${family_name_with_type})
      endif()
      PLATFORM_HAS_VME(${GEN_TYPE} ${PLATFORM_IT} PLATFORM_SUPPORTS_VME)
      if(PLATFORM_SUPPORTS_VME)
        add_dependencies(unit_tests test_kernel_vme_${family_name_with_type})
      endif()
    endif()
  endforeach()
endmacro()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
      set(PLATFORM_LOWER ${DEFAULT_SUPPORTED_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})
      set(PLATFORM_2_0_LOWER ${DEFAULT_SUPPORTED_2_0_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})
      set(PLATFORM_VME_LOWER ${DEFAULT_SUPPORTED_VME_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})

      if(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
        neo_gen_kernels(test_kernels_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${TEST_KERNELS})
        neo_gen_kernels_with_options(test_kernel_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${TEST_KERNEL} ${TEST_KERNEL_options})
        # Temporarily disabled debug kernel generation on gen8
        if(NOT ("${GEN_TYPE_LOWER}" STREQUAL "gen8"))
          neo_gen_kernel_with_kernel_debug_options(test_kernel_debug_enable_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${TEST_KERNEL})
        endif()
        # Gen9lp needs extra -m32 flag
        if( ("${GEN_TYPE_LOWER}" STREQUAL "gen9") AND ("${PLATFORM_TYPE_LOWER}" STREQUAL "lp"))
          neo_gen_kernels_with_internal_options(test_kernel_stateless_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${TEST_KERNEL_STATELESS} ${TEST_KERNEL_STATELESS_internal_options_gen9lp})
        else()
          neo_gen_kernels_with_internal_options(test_kernel_stateless_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${TEST_KERNEL_STATELESS} ${TEST_KERNEL_STATELESS_internal_options})
        endif()
        set(sip_kernel_file_name)
        set(sip_kernel_output_file)
        set(sip_debug_kernel_output_file)
        set(sip_debug_local_kernel_output_file)
        list(APPEND sip_kernel_file_name "test_files/sip_dummy_kernel_${NEO_BITS}.ll")
        list(APPEND sip_debug_kernel_output_file "sip_dummy_kernel_debug_${NEO_BITS}")
        list(APPEND sip_debug_local_kernel_output_file "sip_dummy_kernel_debug_local_${NEO_BITS}")

        # Temporarily disabled sip kernel generation
        # if("${GEN_TYPE_LOWER}" STREQUAL "gen9" )
        #   neo_gen_kernel_from_ll(test_kernel_sip_debug_local_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${sip_kernel_file_name} ${sip_debug_local_kernel_output_file} ${TEST_KERNEL_SIP_DEBUG_LOCAL_options})
        #   add_dependencies(unit_tests test_kernel_sip_debug_local_${family_name_with_type})
        # endif()

        # neo_gen_kernel_from_ll(test_kernel_sip_debug_${family_name_with_type} ${PLATFORM_LOWER} ${family_name_with_type} ${sip_kernel_file_name} ${sip_debug_kernel_output_file} ${TEST_KERNEL_SIP_DEBUG_options})
        # add_dependencies(unit_tests test_kernel_sip_debug_${family_name_with_type})

        if(PLATFORM_2_0_LOWER)
          neo_gen_kernels_with_options(test_kernel_2_0_${family_name_with_type} ${PLATFORM_2_0_LOWER} ${family_name_with_type} "${TEST_KERNEL_2_0}" ${TEST_KERNEL_2_0_options})
        endif()
        if(PLATFORM_VME_LOWER)
          neo_gen_kernels(test_kernel_vme_${family_name_with_type} ${PLATFORM_VME_LOWER} ${family_name_with_type} ${TEST_KERNEL_VME})
        endif()
      endif()
    endif()
  endforeach()
  apply_macro_for_each_platform()
endmacro()
apply_macro_for_each_gen("TESTED")
add_subdirectories()
add_dependencies(unit_tests mock_gmm)
add_dependencies(igdrcl_tests mock_gmm)
create_project_source_tree(igdrcl_tests ${NEO_SOURCE_DIR}/runtime)

set(UltPchHeader "${CMAKE_CURRENT_SOURCE_DIR}/igdrcl_tests_pch.h")
set(UltPchSource "${CMAKE_CURRENT_SOURCE_DIR}/igdrcl_tests_pch.cpp")
get_target_property(UltSources igdrcl_tests SOURCES)

if(MSVC AND NOT DISABLE_ULT_PCH_WIN)
  set(UltPchBinary "${CMAKE_CURRENT_BINARY_DIR}/igdrcl_tests_pch.pch")

  set(IGDRCL_SRCS_ult_pch ${UltPchSource} ${UltPchHeader})
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_ult_pch})

  set_source_files_properties(${UltSources}
    PROPERTIES COMPILE_FLAGS "/Yu${UltPchHeader} /FI${UltPchHeader} /Fp${UltPchBinary}"
    OBJECT_DEPENDS "${UltPchBinary}")

  set_source_files_properties(${UltPchSource}
    PROPERTIES COMPILE_FLAGS "/Yc${UltPchHeader} /FI${UltPchHeader} /Fp${UltPchBinary}"
    OBJECT_OUTPUTS "${UltPchBinary}")
elseif(USE_ULT_PCH)
  set(UltPchHeaderInBuildDir "${CMAKE_CURRENT_BINARY_DIR}/igdrcl_tests_pch.h")
  set(UltPchBinaryGch "${UltPchHeaderInBuildDir}.gch")
  set(UltPchBinary "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/igdrcl_ult_pch.dir/igdrcl_tests_pch.h.o")

  add_library(igdrcl_ult_pch STATIC EXCLUDE_FROM_ALL ${UltPchHeader})
  add_dependencies(igdrcl_tests igdrcl_ult_pch)

  target_include_directories(igdrcl_ult_pch PRIVATE
    $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:igdrcl_tests,INCLUDE_DIRECTORIES>)
  target_compile_definitions(igdrcl_ult_pch PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

  target_include_directories(igdrcl_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  if(NOT USE_ASAN)
    set_source_files_properties(${UltSources}
      PROPERTIES COMPILE_FLAGS "-include ${UltPchHeaderInBuildDir} -msse4"
      OBJECT_DEPENDS ${UltPchBinaryGch})
  endif()

  set_source_files_properties(${UltPchHeader}
    PROPERTIES LANGUAGE "CXX"
    COMPILE_FLAGS "-x c++-header -msse4 -gdwarf-2")

  add_custom_command(
    OUTPUT ${UltPchBinaryGch}
    COMMAND cp "${UltPchHeader}" "${UltPchHeaderInBuildDir}"
    COMMAND cp "${UltPchBinary}" "${UltPchBinaryGch}"
    DEPENDS ${UltPchBinary})
endif()

# Dont add any sources below PCH logic. This is to keep PCH dependencies correctly without creating new target
