# Copyright (c) 2017, Intel Corporation
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

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

project(igdrcl_tests)

add_subdirectory(accelerators)
add_subdirectory(api)
add_subdirectory(built_ins)
add_subdirectory(command_queue)
add_subdirectory(command_stream)
add_subdirectory(compiler_interface)
add_subdirectory(context)
add_subdirectory(device)
add_subdirectory(device_queue)
add_subdirectory(event)
add_subdirectory(execution_model)
add_subdirectory(fixtures)
add_subdirectory(gen_common)
if(GTPIN_HEADERS_DIR)
add_subdirectory(gtpin)
endif(GTPIN_HEADERS_DIR)
add_subdirectory(helpers)
add_subdirectory(indirect_heap)
add_subdirectory(instrumentation)
add_subdirectory(kernel)
add_subdirectory(linux)
add_subdirectory(libult)
add_subdirectory(mem_obj)
add_subdirectory(memory_manager)
add_subdirectory(mocks)
add_subdirectory(offline_compiler)
add_subdirectory(os_interface)
add_subdirectory(platform)
add_subdirectory(preamble)
add_subdirectory(preemption)
add_subdirectory(profiling)
add_subdirectory(program)
add_subdirectory(sampler)
add_subdirectory(scenarios)
add_subdirectory(scheduler)
add_subdirectory(sharings)
add_subdirectory(utilities)

add_subdirectory(test_dynamic_lib)
if(MSVC)
  add_subdirectory(mock_gdi)
  add_subdirectory(d3d_sharing)
endif()

if(NOT GTEST_REPEAT)
  set(GTEST_REPEAT 1 CACHE STRING "Google test iterations")
endif(NOT GTEST_REPEAT)
set(GTEST_REPEAT ${GTEST_REPEAT} CACHE STRING "Google test iterations")

message(STATUS "GTest repeat count set to ${GTEST_REPEAT}")
if(NOT GTEST_SHUFFLE)
  set(GTEST_SHUFFLE --gtest_shuffle --gtest_random_seed=0)
endif(NOT GTEST_SHUFFLE)
message(STATUS "GTest shuffle set to ${GTEST_SHUFFLE}")

set(IGDRCL_SRCS_tests_local
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/abort.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/custom_event_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/global_environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/global_environment.h
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program/evaluate_unhandled_token_ult.cpp
)

set(IGDRCL_SRCS_tests
  ${IGDRCL_SRCS_tests_local}
  ${IGDRCL_SRCS_built_in_tests}
  ${IGDRCL_SRCS_tests_accelerators}
  ${IGDRCL_SRCS_tests_api}
  ${IGDRCL_SRCS_tests_command_queue}
  ${IGDRCL_SRCS_tests_command_stream}
  ${IGDRCL_SRCS_tests_compiler_interface}
  ${IGDRCL_SRCS_tests_context}
  ${IGDRCL_SRCS_tests_device}
  ${IGDRCL_SRCS_tests_device_queue}
  ${IGDRCL_SRCS_tests_event}
  ${IGDRCL_SRCS_tests_execution_model}
  ${IGDRCL_SRCS_tests_fixtures}
  ${IGDRCL_SRCS_tests_gen_common}
  ${IGDRCL_SRCS_tests_helpers}
  ${IGDRCL_SRCS_tests_instrumentation}
  ${IGDRCL_SRCS_tests_indirect_heap}
  ${IGDRCL_SRCS_tests_kernel}
  ${IGDRCL_SRCS_tests_mem_obj}
  ${IGDRCL_SRCS_tests_memory_manager}
  ${IGDRCL_SRCS_tests_mocks}
  ${IGDRCL_SRCS_tests_os_interface}
  ${IGDRCL_SRCS_tests_platform}
  ${IGDRCL_SRCS_tests_preamble}
  ${IGDRCL_SRCS_tests_preemption}
  ${IGDRCL_SRCS_tests_profiling}
  ${IGDRCL_SRCS_tests_program}
  ${IGDRCL_SRCS_tests_sampler}
  ${IGDRCL_SRCS_tests_scenarios}
  ${IGDRCL_SRCS_tests_scheduler}
  ${IGDRCL_SRCS_tests_sharings}
  ${IGDRCL_SRCS_tests_utilities}
)

if(GTPIN_HEADERS_DIR)
  list (APPEND IGDRCL_SRCS_tests
    ${IGDRCL_SRCS_tests_gtpin}
  )
endif(GTPIN_HEADERS_DIR)

if(MSVC)
  list(APPEND IGDRCL_SRCS_tests
    ${IGDRCL_SRCS_tests_d3d_sharing}
  )
endif(MSVC)

if(NOT MSVC)
  set_source_files_properties(helpers/uint16_sse4_tests.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif (NOT MSVC)

if(NEO_DRIVER_VERSION)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/device/device_caps_tests.cpp PROPERTIES COMPILE_DEFINITIONS NEO_DRIVER_VERSION="${NEO_DRIVER_VERSION}")
endif(NEO_DRIVER_VERSION)

link_directories(${GMM_LIB_PATHS})
if(CLELF_LIB_DIR)
  link_directories(${CLELF_LIB_DIR})
endif(CLELF_LIB_DIR)

foreach(GEN_NUM RANGE 0 ${MAX_GEN} 1)
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    add_subdirectory(gen${GEN_NUM})
    list (APPEND IGDRCL_SRCS_tests ${IGDRCL_SRCS_tests_gen${GEN_NUM}})
    source_group("gen${GEN_NUM}" FILES ${IGDRCL_SRCS_tests_gen${GEN_NUM}})
    if(NOT MSVC)
      list (APPEND IGDRCL_SRCS_tests ${IGDRCL_SRCS_tests_gen${GEN_NUM}_linux})
      source_group("gen${GEN_NUM}\\linux" FILES ${IGDRCL_SRCS_tests_gen${GEN_NUM}_linux})
    else(NOT MSVC)
      list (APPEND IGDRCL_SRCS_tests ${IGDRCL_SRCS_tests_gen${GEN_NUM}_windows})
      source_group("gen${GEN_NUM}\\windows" FILES ${IGDRCL_SRCS_tests_gen${GEN_NUM}_windows})
    endif(NOT MSVC)
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_NUM} TESTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      list (APPEND IGDRCL_SRCS_linux_dll_tests ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/hw_info_${PLATFORM_IT_LOWER}.cpp)
      list (APPEND IGDRCL_SRCS_tests ${IGDRCL_SRCS_tests_gen${GEN_NUM}_${PLATFORM_IT_LOWER}})
      source_group("gen${GEN_NUM}\\${PLATFORM_IT_LOWER}" FILES ${IGDRCL_SRCS_tests_gen${GEN_NUM}_${PLATFORM_IT_LOWER}})
      if(NOT MSVC)
        list (APPEND IGDRCL_SRCS_tests ${IGDRCL_SRCS_tests_gen${GEN_NUM}_${PLATFORM_IT_LOWER}_linux})
        source_group("gen${GEN_NUM}\\${PLATFORM_IT_LOWER}\\linux" FILES ${IGDRCL_SRCS_tests_gen${GEN_NUM}_${PLATFORM_IT_LOWER}_linux})
      endif(NOT MSVC)
    endforeach(PLATFORM_IT)
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)

add_executable(igdrcl_tests
  ${IGDRCL_SRCS_tests}
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_lib_mockable_sharings_enable>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
)
target_link_libraries(igdrcl_tests ${NEO_MOCKABLE_LIB_NAME})

option(SHOW_VERBOSE_UTESTS_RESULTS "Use the default/verbose test output" ON)

if(NOT SHOW_VERBOSE_UTESTS_RESULTS)
  set(IGDRCL_TESTS_LISTENER_OPTION "--disable_default_listener")
endif()

set(BUILT_IN_KERNEL_DIR "${IGDRCL_SOURCE_DIR}/runtime/built_ins")

target_include_directories(igdrcl_tests PRIVATE
  ${GMM_INCLUDE_PATHS}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${INSTRUMENTATION_INCLUDE_PATH}
  ${GTEST_INCLUDE_DIR}
  ${GMOCK_INCLUDE_DIR}
)

if(MSVC)
  target_include_directories(igdrcl_tests PUBLIC
    ${WDK_INCLUDE_PATHS}
  )
  message(STATUS "TargetDir is: ${TargetDir}")
  target_link_libraries (igdrcl_tests ${gtest_lib} ${gmock_lib} Ws2_32.lib)
else()
  add_executable(igdrcl_linux
    "main_linux.cpp"
    $<TARGET_OBJECTS:igdrcl_libult_cs>
    ${IGDRCL_SOURCE_DIR}/unit_tests/abort.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_deferred_deleter.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_gmm_resource_info.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp
  )
  add_executable(igdrcl_linux_dll_tests
    ${IGDRCL_SRCS_linux_dll_tests}
    $<TARGET_OBJECTS:igdrcl_libult_cs>
    ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/drm_neo_create.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/abort.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_deferred_deleter.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_gmm_resource_info.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp
  )
  target_link_libraries(igdrcl_linux ${gtest_lib} ${gmock_lib} ${NEO_MOCKABLE_LIB_NAME} dl pthread)
  target_link_libraries(igdrcl_linux_dll_tests ${gtest_lib} ${gmock_lib} ${NEO_MOCKABLE_LIB_NAME} dl pthread)
  target_link_libraries(igdrcl_tests dl pthread ${gtest_lib} ${gmock_lib})

  set_property(TARGET igdrcl_linux_dll_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  set_property(TARGET igdrcl_linux APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  target_include_directories(igdrcl_linux PRIVATE
    ${GTEST_INCLUDE_DIR}
    ${GMOCK_INCLUDE_DIR}
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
    ${GMM_INCLUDE_PATHS}
    ${GMM_SOURCE_DIR}/inc/common
    ${KHRONOS_HEADERS_DIR}
    ${IGDRCL__IGC_INCLUDE_DIR}
    ${THIRD_PARTY_DIR}
  )
  target_include_directories(igdrcl_linux_dll_tests PRIVATE
    ${GTEST_INCLUDE_DIR}
    ${GMOCK_INCLUDE_DIR}
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
    ${GMM_INCLUDE_PATHS}
    ${GMM_SOURCE_DIR}/inc/common
    ${KHRONOS_HEADERS_DIR}
    ${IGDRCL__IGC_INCLUDE_DIR}
    ${THIRD_PARTY_DIR}
  )
endif(MSVC)

function(neo_copy_test_files target gen)
  set(outputdir "${TargetDir}/${gen}")
  add_custom_target(${target})
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND echo deleting and re-creating ${gen} cache directory...
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${outputdir}/cl_cache
    COMMAND ${CMAKE_COMMAND} -E make_directory ${outputdir}/cl_cache
    COMMAND echo copying built-in kernel files from ${BUILT_IN_KERNEL_DIR}/kernels to ${outputdir}/test_files
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BUILT_IN_KERNEL_DIR}/kernels ${outputdir}/test_files
    COMMAND echo copying test files from ${CMAKE_CURRENT_SOURCE_DIR}/test_files to ${outputdir}/test_files
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/test_files ${outputdir}/test_files
    COMMAND WORKING_DIRECTORY ${TargetDir}
  )
  add_dependencies(${target} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "opencl runtime/${gen}")
endfunction()

add_custom_target(unit_tests
    DEPENDS igdrcl_tests
    DEPENDS test_dynamic_lib
)

add_custom_target(run_unit_tests ALL)

if(MSVC)
  add_dependencies(unit_tests mock_gdi)
else (MSVC)
  add_dependencies(unit_tests igdrcl_linux igdrcl_linux_dll_tests)
  add_custom_command(
    TARGET run_unit_tests
    POST_BUILD
    COMMAND echo running linux specific tests for static library in ${TargetDir}
    COMMAND igdrcl_linux ${IGDRCL_TESTS_LISTENER_OPTION}
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
  endif(PRE_ULT_COMMAND)
endif (MSVC)

source_group("source files" FILES ${IGDRCL_SRCS_tests_local})
source_group("source files\\accelerators" FILES ${IGDRCL_SRCS_tests_accelerators})
source_group("source files\\api" FILES ${IGDRCL_SRCS_tests_api})
source_group("source files\\built_ins" FILES ${IGDRCL_SRCS_built_in_tests})
source_group("source files\\command_queue" FILES ${IGDRCL_SRCS_tests_command_queue})
source_group("source files\\command_stream" FILES ${IGDRCL_SRCS_tests_command_stream})
source_group("source files\\compiler_interface" FILES ${IGDRCL_SRCS_tests_compiler_interface})
source_group("source files\\context" FILES ${IGDRCL_SRCS_tests_context})
source_group("source files\\device" FILES ${IGDRCL_SRCS_tests_device})
source_group("source files\\device_queue" FILES ${IGDRCL_SRCS_tests_device_queue})
source_group("source files\\event" FILES ${IGDRCL_SRCS_tests_event})
source_group("source files\\execution_model" FILES ${IGDRCL_SRCS_tests_execution_model})
source_group("source files\\fixtures" FILES ${IGDRCL_SRCS_tests_fixtures})
source_group("source files\\gen_common" FILES ${IGDRCL_SRCS_tests_gen_common})
source_group("source files\\helpers" FILES ${IGDRCL_SRCS_tests_helpers})
source_group("source files\\indirect_heap" FILES ${IGDRCL_SRCS_tests_indirect_heap})
source_group("source files\\instrumentation" FILES ${IGDRCL_SRCS_tests_instrumentation})
source_group("source files\\kernel" FILES ${IGDRCL_SRCS_tests_kernel})
source_group("source files\\mem_obj" FILES ${IGDRCL_SRCS_tests_mem_obj})
source_group("source files\\memory_manager" FILES ${IGDRCL_SRCS_tests_memory_manager})
source_group("source files\\mocks" FILES ${IGDRCL_SRCS_tests_mocks})
source_group("source files\\os_interface" FILES ${IGDRCL_SRCS_tests_os_interface})
source_group("source files\\platform" FILES ${IGDRCL_SRCS_tests_platform})
source_group("source files\\preamble" FILES ${IGDRCL_SRCS_tests_preamble})
source_group("source files\\preemption" FILES ${IGDRCL_SRCS_tests_preemption})
source_group("source files\\profiling" FILES ${IGDRCL_SRCS_tests_profiling})
source_group("source files\\program" FILES ${IGDRCL_SRCS_tests_program})
source_group("source files\\sampler" FILES ${IGDRCL_SRCS_tests_sampler})
source_group("source files\\scenarios" FILES ${IGDRCL_SRCS_tests_scenarios})
source_group("source files\\scheduler" FILES ${IGDRCL_SRCS_tests_scheduler})
source_group("source files\\sharings" FILES ${IGDRCL_SRCS_tests_sharings})
source_group("source files\\utilities" FILES ${IGDRCL_SRCS_tests_utilities})

if(MSVC)
  source_group("source files\\d3d_sharing" FILES ${IGDRCL_SRCS_tests_d3d_sharing})
endif()
set_target_properties(igdrcl_tests PROPERTIES FOLDER "opencl runtime")
set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
if(NOT MSVC)
  set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()

set_target_properties(unit_tests PROPERTIES FOLDER "opencl runtime")
set_target_properties(run_unit_tests PROPERTIES FOLDER "opencl runtime")

target_compile_definitions(igdrcl_tests PUBLIC ${TESTED_GEN_FLAGS_DEFINITONS})

if(NOT MSVC)
  target_include_directories(igdrcl_linux BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common ${IGDRCL_SOURCE_DIR}/runtime/gen_common)
  target_include_directories(igdrcl_linux_dll_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common ${IGDRCL_SOURCE_DIR}/runtime/gen_common)
endif()
target_include_directories(igdrcl_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common ${IGDRCL_SOURCE_DIR}/runtime/gen_common)

function(neo_gen_kernels target gen)
  set(outputdir "${TargetDir}/${gen}/test_files/${NEO_ARCH}/")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  foreach(filepath ${ARGN})
    get_filename_component(filename ${filepath} NAME)
    get_filename_component(basename ${filepath} NAME_WE)

    set(outputpath_base "${outputdir}${basename}_${gen}")
    set(output_files
      ${outputpath_base}.bc
      ${outputpath_base}.bin
      ${outputpath_base}.gen
    )

    if(MSVC)
      add_custom_command(
          OUTPUT ${output_files}
          COMMAND cloc -q -file ${filename} -device ${gen} -${NEO_BITS} -out_dir ${outputdir}
          WORKING_DIRECTORY ${workdir}
          DEPENDS ${filepath} cloc
      )
    else()
      add_custom_command(
          OUTPUT ${output_files}
          COMMAND LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc> -q -file ${filename} -device ${gen} -${NEO_BITS} -out_dir ${outputdir}
          WORKING_DIRECTORY ${workdir}
          DEPENDS ${filepath} cloc
      )
    endif()

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${gen}")
endfunction()

function(neo_gen_kernel_with_options target gen filepath)
  get_filename_component(filename ${filepath} NAME)
  get_filename_component(basename ${filepath} NAME_WE)

  set(outputdir "${TargetDir}/${gen}/test_files/${NEO_ARCH}/")
  set(workdir "${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  set(results)
  foreach(arg ${ARGN})
    string(REPLACE " " "_" argwospaces ${arg})

    set(outputpath_base "${outputdir}/${basename}_${gen}")
    set(output_files
      ${outputpath_base}.bc${argwospaces}
      ${outputpath_base}.bin${argwospaces}
      ${outputpath_base}.gen${argwospaces}
    )
    set(cmd "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc> -q -file ${filename} -device ${gen} -${NEO_BITS} -out_dir ${outputdir} -options ${arg} -options_name")

    if(MSVC)
      add_custom_command(
          OUTPUT ${output_files}
          COMMAND cloc -q -file ${filename} -device ${gen} -${NEO_BITS} -out_dir ${outputdir} -options ${arg} -options_name
          WORKING_DIRECTORY ${workdir}
          DEPENDS ${filepath} cloc
      )
    else()
      add_custom_command(
          OUTPUT ${output_files}
          COMMAND LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc> -q -file ${filename} -device ${gen} -${NEO_BITS} -out_dir ${outputdir} -options ${arg} -options_name
          WORKING_DIRECTORY ${workdir}
          DEPENDS ${filepath} cloc
      )
    endif()

    list(APPEND results ${output_files})
  endforeach()
  add_custom_target(${target} DEPENDS ${results} copy_compiler_files)
  set_target_properties(${target} PROPERTIES FOLDER "kernels/${gen}")
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
      COMMAND ${TargetDir}/verify.bat --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${IGDRCL_TESTS_LISTENER_OPTION}
      COMMAND echo App Verifier returned: %errorLevel%
    )
  endif()
endfunction(gen_run_tests_with_appverifier)

function(gen_run_unit_tests product slices subslices eu_per_ss)
  add_custom_target(run_${product}_unit_tests ALL DEPENDS unit_tests)
  neo_copy_test_files(copy_test_files_${product} ${product})
  add_dependencies(unit_tests copy_test_files_${product})
  set_target_properties(run_${product}_unit_tests PROPERTIES FOLDER "opencl runtime/${product}")
  add_custom_command(
    TARGET run_${product}_unit_tests
    POST_BUILD
    COMMAND WORKING_DIRECTORY ${TargetDir}
    COMMAND echo Running igdrcl_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
    COMMAND igdrcl_tests --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${IGDRCL_TESTS_LISTENER_OPTION}
  )
  add_dependencies(run_unit_tests run_${product}_unit_tests)
endfunction(gen_run_unit_tests)

foreach(GEN_NUM RANGE 0 ${MAX_GEN} 1)
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
        add_dependencies(unit_tests test_kernels_${PLATFORM_IT_LOWER})
        add_dependencies(unit_tests test_kernel_${PLATFORM_IT_LOWER})
        if(${PLATFORM_SUPPORTS_2_0})
          neo_gen_kernel_with_options(test_kernel_2_0_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${TEST_KERNEL_2_0} ${TEST_KERNEL_2_0_options})
          add_dependencies(unit_tests test_kernel_2_0_${PLATFORM_IT_LOWER})
        endif(${PLATFORM_SUPPORTS_2_0})
      endif(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
      GET_TEST_CONFIGURATIONS_FOR_PLATFORM("UNIT_TESTS" ${GEN_NUM} ${PLATFORM_IT} PLATFORM_CONFIGURATIONS)
      foreach(PLATFORM_CONFIGURATION ${PLATFORM_CONFIGURATIONS})
        string(REPLACE "/" ";" CONFIGURATION_PARAMS ${PLATFORM_CONFIGURATION})
        list(GET CONFIGURATION_PARAMS 1 SLICES)
        list(GET CONFIGURATION_PARAMS 2 SUBSLICES)
        list(GET CONFIGURATION_PARAMS 3 EU_PER_SS)
        gen_run_unit_tests(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
        if(MSVC AND ${TESTED_WITH_APPVERIFIER})
          gen_run_tests_with_appverifier(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
        endif(MSVC AND ${TESTED_WITH_APPVERIFIER})
      endforeach(PLATFORM_CONFIGURATION)
    endforeach(PLATFORM_IT)
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)

add_subdirectory(elflib)
add_subdirectory(mt_tests)
add_subdirectory(aub_tests)
add_subdirectory(tbx)
