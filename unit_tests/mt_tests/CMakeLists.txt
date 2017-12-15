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

project(igdrcl_mt_tests)

add_subdirectory(api)
add_subdirectory(command_queue)
add_subdirectory(device_queue)
add_subdirectory(event)
add_subdirectory(fixtures)
add_subdirectory(helpers)
add_subdirectory(indirect_heap)
add_subdirectory(memory_manager)
add_subdirectory(mocks)
add_subdirectory(os_interface)
add_subdirectory(platform)
add_subdirectory(utilities)

set(IGDRCL_SRCS_mt_tests_local
  #local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  #necessary dependencies from igdrcl_tests
  ${IGDRCL_SOURCE_DIR}/unit_tests/abort.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/base_object_allocator.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/global_environment.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/main.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/memory_leak_listener.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/ult_configuration.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/program_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_cif.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_compilers.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_gmm_resource_info.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp
)

# Setting up our local list of test files
set(IGDRCL_SRCS_mt_tests
  ${IGDRCL_SRCS_gtest}
  ${IGDRCL_SRCS_mt_tests_api}
  ${IGDRCL_SRCS_mt_tests_command_queue}
  ${IGDRCL_SRCS_mt_tests_device_queue}
  ${IGDRCL_SRCS_mt_tests_event}
  ${IGDRCL_SRCS_mt_tests_fixtures}
  ${IGDRCL_SRCS_mt_tests_helpers}
  ${IGDRCL_SRCS_mt_tests_indirect_heap}
  ${IGDRCL_SRCS_mt_tests_local}
  ${IGDRCL_SRCS_mt_tests_memory_manager}
  ${IGDRCL_SRCS_mt_tests_mocks}
  ${IGDRCL_SRCS_mt_tests_os_interface}
  ${IGDRCL_SRCS_mt_tests_platform}
  ${IGDRCL_SRCS_mt_tests_utilities}
)

if(MSVC)
  list(APPEND IGDRCL_SRCS_mt_tests
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/mock_cif.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_gmm_page_table_mngr.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks/mock_gmm_memory.cpp
  )
else(MSVC)
  list(APPEND IGDRCL_SRCS_mt_tests
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/mock_cif.cpp
    
  )
endif(MSVC)

#these tests fail because of data race, set '*' after fix problem
set(GTEST_FILTER "-AsyncGPUoperations.MapBufferAfterWriteBuffer")
MESSAGE(STATUS "GTest filter for mt tests:" ${GTEST_FILTER} )

add_executable(igdrcl_mt_tests EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_mt_tests}
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
)
target_link_libraries(igdrcl_mt_tests ${TSAN_LIB})
target_link_libraries(igdrcl_mt_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_mt_tests gtest gmock)

target_include_directories(igdrcl_mt_tests PRIVATE
  ${GTEST_INCLUDE_DIR}
  ${GMOCK_INCLUDE_DIR}
  ${GMM_INCLUDE_PATHS}
)

option(SHOW_VERBOSE_UTESTS_RESULTS "Use the default/verbose test output" ON)

if(NOT SHOW_VERBOSE_UTESTS_RESULTS)
  set(igdrcl_mt_tests_LISTENER_OPTION "--disable_default_listener")
endif()

if(MSVC)
  target_include_directories(igdrcl_mt_tests PRIVATE
    ${WDK_INCLUDE_PATHS}
    ${GMM_INCLUDE_PATHS}
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  )
  target_link_libraries (igdrcl_mt_tests Ws2_32.lib)
else()
  target_include_directories(igdrcl_mt_tests PRIVATE
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  )
  target_link_libraries(igdrcl_mt_tests dl pthread)
endif(MSVC)

if(MSVC)
  add_dependencies(igdrcl_mt_tests mock_gdi igdrcl_tests)
endif()

add_custom_target(run_mt_unit_tests)

add_dependencies(igdrcl_mt_tests test_dynamic_lib)

source_group("source files" FILES ${IGDRCL_SRCS_mt_tests_local})
source_group("source files\\api" FILES ${IGDRCL_SRCS_mt_tests_api})
source_group("source files\\command_queue" FILES ${IGDRCL_SRCS_mt_tests_command_queue})
source_group("source files\\device_queue" FILES ${IGDRCL_SRCS_mt_tests_device_queue})
source_group("source files\\event" FILES ${IGDRCL_SRCS_mt_tests_event})
source_group("source files\\fixtures" FILES ${IGDRCL_SRCS_mt_tests_fixtures})
source_group("source files\\helpers" FILES ${IGDRCL_SRCS_mt_tests_helpers})
source_group("source files\\indirect_heap" FILES ${IGDRCL_SRCS_mt_tests_indirect_heap})
source_group("source files\\memory_manager" FILES ${IGDRCL_SRCS_mt_tests_memory_manager})
source_group("source files\\mocks" FILES ${IGDRCL_SRCS_mt_tests_mocks})
source_group("source files\\os_interface" FILES ${IGDRCL_SRCS_mt_tests_os_interface})
source_group("source files\\platform" FILES ${IGDRCL_SRCS_mt_tests_platform})
source_group("source files\\utilities" FILES ${IGDRCL_SRCS_mt_tests_utilities})

set_target_properties(igdrcl_mt_tests PROPERTIES FOLDER "opencl runtime")
set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${TSAN_FLAGS})
if(NOT MSVC)
  set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()

set_target_properties(run_mt_unit_tests PROPERTIES FOLDER "opencl runtime")

function(run_mt_tests target slices subslices eu_per_ss)
  add_custom_target(run_${target}_mt_unit_tests DEPENDS igdrcl_mt_tests)
  if(NOT MSVC)
    add_dependencies(run_${target}_mt_unit_tests copy_test_files_${target})
  endif()
  add_dependencies(run_mt_unit_tests run_${target}_mt_unit_tests)
  set_target_properties(run_${target}_mt_unit_tests PROPERTIES FOLDER "opencl runtime/${target}")

  add_custom_command(
    TARGET run_${target}_mt_unit_tests
    POST_BUILD
    COMMAND WORKING_DIRECTORY ${TargetDir}
    COMMAND echo "Running igdrcl_mt_tests ${target} ${slices}x${subslices}x${eu_per_ss}"
    COMMAND igdrcl_mt_tests --product ${target} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=${GTEST_REPEAT} ${igdrcl_mt_tests_LISTENER_OPTION} --gtest_filter=${GTEST_FILTER}
  )
endfunction()

target_include_directories(igdrcl_mt_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common ${IGDRCL_SOURCE_DIR}/runtime/gen_common)

target_compile_definitions(igdrcl_mt_tests PUBLIC ${TESTED_GEN_FLAGS_DEFINITONS})
foreach(GEN_NUM RANGE 0 ${MAX_GEN} 1)
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_NUM} TESTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      if(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_dependencies(igdrcl_mt_tests test_kernels_${PLATFORM_IT_LOWER})
      endif(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
      GET_TEST_CONFIGURATIONS_FOR_PLATFORM("MT_TESTS" ${GEN_NUM} ${PLATFORM_IT} PLATFORM_CONFIGURATIONS)
      foreach(PLATFORM_CONFIGURATION ${PLATFORM_CONFIGURATIONS})
        string(REPLACE "/" ";" CONFIGURATION_PARAMS ${PLATFORM_CONFIGURATION})
        list(GET CONFIGURATION_PARAMS 1 SLICES)
        list(GET CONFIGURATION_PARAMS 2 SUBSLICES)
        list(GET CONFIGURATION_PARAMS 3 EU_PER_SS)
        run_mt_tests(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
      endforeach(PLATFORM_CONFIGURATION)
    endforeach(PLATFORM_IT)
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)
