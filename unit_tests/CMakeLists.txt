# Copyright (c) 2017 - 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

project(igdrcl_tests)

set(TEST_PROJECTS_FOLDER "opencl runtime")
set(PLATFORM_SPECIFIC_TARGETS_FOLDER "${TEST_PROJECTS_FOLDER}/test platforms")

add_custom_target(unit_tests)
add_custom_target(run_unit_tests ALL)

set(IGDRCL_SRCS_tests_local
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/custom_event_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/global_environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/global_environment.h
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_configuration.cpp
)

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
  $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
)

add_executable(igdrcl_tests
  ${NEO_IGDRCL_TESTS__TARGET_OBJECTS}
  ${IGDRCL_SRCS_tests_local}
)

add_subdirectories()

if(NOT GTEST_REPEAT)
  set(GTEST_REPEAT 1 CACHE STRING "Google test iterations")
endif()
set(GTEST_REPEAT ${GTEST_REPEAT} CACHE STRING "Google test iterations")

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

if(NEO_DRIVER_VERSION)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/device/device_caps_tests.cpp PROPERTIES COMPILE_DEFINITIONS NEO_DRIVER_VERSION="${NEO_DRIVER_VERSION}")
endif()

target_link_libraries(igdrcl_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_tests igdrcl_mocks)

option(SHOW_VERBOSE_UTESTS_RESULTS "Use the default/verbose test output" ON)

if(NOT SHOW_VERBOSE_UTESTS_RESULTS)
  set(IGDRCL_TESTS_LISTENER_OPTION "--disable_default_listener")
endif()

target_include_directories(igdrcl_tests PRIVATE
  ${GTEST_INCLUDE_DIR}
  ${GMOCK_INCLUDE_DIR}
  ${IGDRCL_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
)

target_link_libraries(igdrcl_tests gtest gmock ${IGDRCL_EXTRA_LIBS})

set(BUILT_IN_KERNEL_DIR "${IGDRCL_SOURCE_DIR}/runtime/built_ins")

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
    COMMAND echo copying test files from ${CMAKE_CURRENT_SOURCE_DIR}/test_files to ${outputdir}/test_files
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/test_files ${outputdir}/test_files
    COMMAND WORKING_DIRECTORY ${TargetDir}
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

target_include_directories(igdrcl_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common)

if(WIN32)
  set(cloc_cmd_prefix cloc)
else()
  set(cloc_cmd_prefix LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc>)
endif()

function(neo_gen_kernels target product)
  set(outputdir "${TargetDir}/${product}/test_files/${NEO_ARCH}/")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)

    set(outputpath_base "${outputdir}${basename}_${product}")
    set(output_files
      ${outputpath_base}.bc
      ${outputpath_base}.bin
      ${outputpath_base}.gen
    )

    add_custom_command(
      OUTPUT ${output_files}
      COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${product} -${NEO_BITS} -out_dir ${outputdir}
      WORKING_DIRECTORY ${workdir}
      DEPENDS ${filepath} cloc
    )

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${product}")
endfunction()

function(neo_gen_kernel_with_options target product filepath)
  get_filename_component(filename ${filepath} NAME)
  get_filename_component(basename ${filepath} NAME_WE)

  set(outputdir "${TargetDir}/${product}/test_files/${NEO_ARCH}/")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  foreach(arg ${ARGN})
    string(REPLACE " " "_" argwospaces ${arg})

    set(outputpath_base "${outputdir}/${basename}_${product}")
    set(output_files
      ${outputpath_base}.bc${argwospaces}
      ${outputpath_base}.bin${argwospaces}
      ${outputpath_base}.gen${argwospaces}
    )

    add_custom_command(
      OUTPUT ${output_files}
      COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${product} -${NEO_BITS} -out_dir ${outputdir} -options ${arg} -options_name
      WORKING_DIRECTORY ${workdir}
      DEPENDS ${filepath} cloc
    )

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${product}")
endfunction()

function(neo_gen_kernel_with_internal_options target product filepath)
  get_filename_component(filename ${filepath} NAME)
  get_filename_component(basename ${filepath} NAME_WE)

  set(outputdir "${TargetDir}/${product}/test_files/${NEO_ARCH}/")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  foreach(arg ${ARGN})
    string(REPLACE " " "_" argwospaces ${arg})

    set(outputpath_base "${outputdir}/${argwospaces}_${product}")
    set(output_files
      ${outputpath_base}.bc
      ${outputpath_base}.bin
      ${outputpath_base}.gen
    )

    add_custom_command(
      OUTPUT ${output_files}
      COMMAND ${cloc_cmd_prefix} -q -file ${filename} -device ${product} -${NEO_BITS} -out_dir ${outputdir} -output ${argwospaces} -internal_options ${arg} -options_name
      WORKING_DIRECTORY ${workdir}
      DEPENDS ${filepath} cloc
    )

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${product}")
endfunction()

function(neo_gen_kernel_from_ll target product filepath output_name compile_options)
  get_filename_component(filename ${filepath} NAME)
  get_filename_component(basename ${filepath} NAME_WE)

  set(outputdir "${TargetDir}/${product}/test_files/${NEO_ARCH}")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  set(outputpath_base "${outputdir}/${output_name}_${product}")
  set(output_files
    ${outputpath_base}.bin
    ${outputpath_base}.gen
  )
  string(CONCAT compile_options \" ${compile_options} \" )

  add_custom_command(
    OUTPUT ${output_files}
    COMMAND ${cloc_cmd_prefix} -q -file ${filename} -output ${output_name} -device ${product} -${NEO_BITS} -out_dir ${outputdir} -internal_options ${compile_options} -llvm_input
    WORKING_DIRECTORY ${workdir}
    DEPENDS ${filepath} cloc
  )

  list(APPEND results ${output_files})

  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${product}")
endfunction()

set(TEST_KERNEL test_files/CopyBuffer_simd8.cl)

set(TEST_KERNEL_options
  "-cl-fast-relaxed-math"
  "-cl-finite-math-only"
  "-cl-kernel-arg-info"
  "-x spir -spir-std=1.2"
)

set(TEST_KERNEL_internal_options
  "-cl-kernel-debug-enable"
)

set(TEST_KERNEL_2_0_options
  "-cl-std=CL2.0"
)

set(TEST_KERNEL_2_0
  test_files/simple_block_kernel.cl
)

set(TEST_KERNEL_SIP_DEBUG_options
  "-cl-include-sip-kernel-debug -cl-include-sip-csr -cl-set-bti:0"
)

set(TEST_KERNEL_SIP_DEBUG_LOCAL_options
  "-cl-include-sip-kernel-local-debug -cl-include-sip-csr -cl-set-bti:0"
)

set(TEST_KERNELS
  test_files/15895692906525787409.cl
  test_files/copybuffer.cl
  test_files/CopyBuffer_simd16.cl
  test_files/CopyBuffer_simd32.cl
  test_files/CopyBuffer_simd8.cl
  test_files/copybuffer_with_header.cl
  test_files/kernel_data_param.cl
  test_files/kernel_num_args.cl
  test_files/media_kernels_backend.cl
  test_files/media_kernels_frontend.cl
  test_files/printf.cl
  test_files/required_work_group.cl
  test_files/simple_arg_int.cl
  test_files/simple_kernels.cl
  test_files/simple_kernels_opts.cl
  test_files/test_basic_constant.cl
  test_files/test_basic_kernel_memory_alignment_private.cl
  test_files/test_constant_memory.cl
  test_files/vme_kernels.cl
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
    COMMAND igdrcl_tests --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
  )
  add_dependencies(run_unit_tests run_${product}_unit_tests)
endfunction()

foreach(GEN_NUM RANGE ${MAX_GEN})
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_NUM} TESTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      PLATFORM_HAS_2_0(${GEN_NUM} ${PLATFORM_IT} PLATFORM_SUPPORTS_2_0)
      PLATFORM_TESTED_WITH_APPVERIFIER(${GEN_NUM} ${PLATFORM_IT} TESTED_WITH_APPVERIFIER)
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

      if(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
        neo_gen_kernels(test_kernels_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${TEST_KERNELS})
        neo_gen_kernel_with_options(test_kernel_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${TEST_KERNEL} ${TEST_KERNEL_options})

        # Temporarily disabled debug kernel generation on gen8
        if(NOT ("gen${GEN_NUM}" STREQUAL "gen8"))
          neo_gen_kernel_with_internal_options(test_kernel_internal_options_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${TEST_KERNEL} ${TEST_KERNEL_internal_options})
        endif()

        add_dependencies(unit_tests test_kernels_${PLATFORM_IT_LOWER})
        add_dependencies(unit_tests test_kernel_${PLATFORM_IT_LOWER})

        # Temporarily disabled debug kernel generation on gen8
        if(NOT ("gen${GEN_NUM}" STREQUAL "gen8"))
          add_dependencies(unit_tests test_kernel_internal_options_${PLATFORM_IT_LOWER})
        endif()

        set(sip_kernel_file_name)
        set(sip_kernel_output_file)
        set(sip_debug_kernel_output_file)
        set(sip_debug_local_kernel_output_file)
        list(APPEND sip_kernel_file_name "test_files/sip_dummy_kernel_${NEO_BITS}.ll")
        list(APPEND sip_debug_kernel_output_file "sip_dummy_kernel_debug_${NEO_BITS}")
        list(APPEND sip_debug_local_kernel_output_file "sip_dummy_kernel_debug_local_${NEO_BITS}")

        # Temporarily disabled sip kernel generation
        # if("gen${GEN_NUM}" STREQUAL "gen9" )
        #   neo_gen_kernel_from_ll(test_kernel_sip_debug_local_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${sip_kernel_file_name} ${sip_debug_local_kernel_output_file} ${TEST_KERNEL_SIP_DEBUG_LOCAL_options})
        #   add_dependencies(unit_tests test_kernel_sip_debug_local_${PLATFORM_IT_LOWER})
        # endif()

        # neo_gen_kernel_from_ll(test_kernel_sip_debug_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${sip_kernel_file_name} ${sip_debug_kernel_output_file} ${TEST_KERNEL_SIP_DEBUG_options})
        # add_dependencies(unit_tests test_kernel_sip_debug_${PLATFORM_IT_LOWER})

        if(${PLATFORM_SUPPORTS_2_0})
          neo_gen_kernel_with_options(test_kernel_2_0_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${TEST_KERNEL_2_0} ${TEST_KERNEL_2_0_options})
          add_dependencies(unit_tests test_kernel_2_0_${PLATFORM_IT_LOWER})
        endif()
      endif()
      GET_TEST_CONFIGURATIONS_FOR_PLATFORM("UNIT_TESTS" ${GEN_NUM} ${PLATFORM_IT} PLATFORM_CONFIGURATIONS)
      foreach(PLATFORM_CONFIGURATION ${PLATFORM_CONFIGURATIONS})
        string(REPLACE "/" ";" CONFIGURATION_PARAMS ${PLATFORM_CONFIGURATION})
        list(GET CONFIGURATION_PARAMS 1 SLICES)
        list(GET CONFIGURATION_PARAMS 2 SUBSLICES)
        list(GET CONFIGURATION_PARAMS 3 EU_PER_SS)
        gen_run_unit_tests(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
        if(MSVC AND ${TESTED_WITH_APPVERIFIER})
          gen_run_tests_with_appverifier(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
        endif()
      endforeach()
    endforeach()
  endif()
endforeach()
create_project_source_tree(igdrcl_tests ${IGDRCL_SOURCE_DIR}/runtime)
