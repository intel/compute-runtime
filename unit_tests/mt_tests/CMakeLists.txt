#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_mt_tests)

add_executable(igdrcl_mt_tests EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${NEO_SOURCE_DIR}/unit_tests/libult/os_interface.cpp
  ${NEO_SOURCE_DIR}/unit_tests/ult_configuration.cpp
  ${NEO_SOURCE_DIR}/runtime/aub/aub_stream_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
  $<TARGET_OBJECTS:mock_gmm>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
)

target_include_directories(igdrcl_mt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectories()

target_link_libraries(igdrcl_mt_tests ${TSAN_LIB})
target_link_libraries(igdrcl_mt_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_mt_tests gmock-gtest)
target_link_libraries(igdrcl_mt_tests igdrcl_mocks ${IGDRCL_EXTRA_LIBS})

if(WIN32)
  target_sources(igdrcl_mt_tests PRIVATE
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
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

create_project_source_tree(igdrcl_mt_tests ${NEO_SOURCE_DIR}/runtime ${NEO_SOURCE_DIR}/unit_tests)

set_target_properties(igdrcl_mt_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${TSAN_FLAGS})
if(NOT WIN32)
  set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()

set_target_properties(run_mt_unit_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

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
    COMMAND igdrcl_mt_tests --product ${target} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=${GTEST_REPEAT} ${igdrcl_mt_tests_LISTENER_OPTION}
  )
endfunction()

target_include_directories(igdrcl_mt_tests BEFORE PRIVATE ${NEO_SOURCE_DIR}/core/unit_tests/test_macros${BRANCH_DIR_SUFFIX} ${NEO_SOURCE_DIR}/runtime/gen_common)

macro(macro_for_each_test_config)
  run_mt_tests(${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
  if(MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8)
    foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
      if(${PLATFORM_IT}_IS_${PLATFORM_TYPE})
        get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
        add_dependencies(run_${PLATFORM_IT_LOWER}_mt_unit_tests prepare_test_kernels_${family_name_with_type})
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
