#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_mt_tests)

add_executable(igdrcl_mt_tests EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

  ${IGDRCL_SOURCE_DIR}/unit_tests/ult_configuration.cpp

  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
)

add_subdirectories()

#these tests fail because of data race, set '*' after fix problem
set(GTEST_FILTER "-AsyncGPUoperations.MapBufferAfterWriteBuffer")
MESSAGE(STATUS "GTest filter for mt tests:" ${GTEST_FILTER} )

target_link_libraries(igdrcl_mt_tests ${TSAN_LIB})
target_link_libraries(igdrcl_mt_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_mt_tests gmock-gtest)
target_link_libraries(igdrcl_mt_tests igdrcl_mocks ${IGDRCL_EXTRA_LIBS})

if(WIN32)
  target_sources(igdrcl_mt_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
  )
endif()

option(SHOW_VERBOSE_UTESTS_RESULTS "Use the default/verbose test output" ON)

if(NOT SHOW_VERBOSE_UTESTS_RESULTS)
  set(igdrcl_mt_tests_LISTENER_OPTION "--disable_default_listener")
endif()

if(WIN32)
  add_dependencies(igdrcl_mt_tests mock_gdi igdrcl_tests)
endif()

add_custom_target(run_mt_unit_tests)

add_dependencies(igdrcl_mt_tests test_dynamic_lib)

create_project_source_tree(igdrcl_mt_tests ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)

set_target_properties(igdrcl_mt_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${TSAN_FLAGS})
if(NOT WIN32)
  set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()

set_target_properties(run_mt_unit_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

if(NOT USE_STATIC_GMM)
  add_dependencies(igdrcl_mt_tests mock_gmm)
endif()

function(run_mt_tests target slices subslices eu_per_ss)
  add_custom_target(run_${target}_mt_unit_tests DEPENDS igdrcl_mt_tests)
  if(NOT WIN32)
    add_dependencies(run_${target}_mt_unit_tests copy_test_files_${target})
  endif()
  add_dependencies(run_mt_unit_tests run_${target}_mt_unit_tests)
  set_target_properties(run_${target}_mt_unit_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${target}")

  add_custom_command(
    TARGET run_${target}_mt_unit_tests
    POST_BUILD
    COMMAND WORKING_DIRECTORY ${TargetDir}
    COMMAND echo "Running igdrcl_mt_tests ${target} ${slices}x${subslices}x${eu_per_ss}"
    COMMAND igdrcl_mt_tests --product ${target} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=${GTEST_REPEAT} ${igdrcl_mt_tests_LISTENER_OPTION} --gtest_filter=${GTEST_FILTER}
  )
endfunction()

target_include_directories(igdrcl_mt_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common ${IGDRCL_SOURCE_DIR}/runtime/gen_common)

macro(macro_for_each_test_config)
  run_mt_tests(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
  if(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
    foreach(PLATFORM_TYPE "CORE" "LP")
      if(${PLATFORM_IT}_IS_${PLATFORM_TYPE})
        get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
        add_dependencies(run_${PLATFORM_IT_LOWER}_mt_unit_tests test_kernels_${family_name_with_type})
      endif()
    endforeach()
  endif()
endmacro()

macro(macro_for_each_platform)
  apply_macro_for_each_test_config("MT_TESTS")
endmacro()

macro(macro_for_each_gen)
  apply_macro_for_each_platform()
endmacro()

apply_macro_for_each_gen("TESTED")
