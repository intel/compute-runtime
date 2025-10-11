#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# Build only if MT tests are not explicitly skipped
if(NOT NEO_SKIP_MT_UNIT_TESTS)
  project(neo_shared_mt_tests)

  include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

  set(SHARED_MT_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  add_custom_target(run_shared_mt_unit_tests)

  add_executable(neo_shared_mt_tests EXCLUDE_FROM_ALL
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${NEO_SHARED_TEST_DIRECTORY}/common/common_main.cpp
                 ${NEO_SHARED_DIRECTORY}/helpers/allow_deferred_deleter.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_sip_listener.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/virtual_file_system_listener.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/tests_configuration.h
                 ${NEO_SHARED_TEST_DIRECTORY}/unit_test/api_specific_config_ult.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/unit_test/ult_specific_config.cpp
                 $<TARGET_OBJECTS:mock_aubstream>
                 $<TARGET_OBJECTS:mock_gmm>
                 $<TARGET_OBJECTS:neo_libult_common>
                 $<TARGET_OBJECTS:neo_libult_cs>
                 $<TARGET_OBJECTS:neo_libult>
                 $<TARGET_OBJECTS:neo_shared_mocks>
                 $<TARGET_OBJECTS:neo_mt_tests_config>
                 $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  )

  target_include_directories(neo_shared_mt_tests PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/mt_tests
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                             ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/includes${BRANCH_DIR_SUFFIX}
  )

  target_sources(neo_shared_mt_tests PRIVATE
                 ${NEO_SHARED_TEST_DIRECTORY}/unit_test/debug_settings/debug_manager.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/unit_test/mocks/mock_gmm_resource_info.cpp
  )

  if(MSVC)
    set_target_properties(neo_shared_mt_tests PROPERTIES
                          VS_DEBUGGER_COMMAND_ARGUMENTS "--gtest_filter=* --gtest_catch_exceptions=0 --enable_default_listener --disable_pagefaulting_tests"
                          VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )
  endif()

  add_dependencies(neo_shared_mt_tests test_dynamic_lib prepare_test_kernels_for_shared)
  add_subdirectories()

  target_link_libraries(neo_shared_mt_tests ${TSAN_LIB})

  target_link_libraries(neo_shared_mt_tests
                        ${NEO_MOCKABLE_LIB_NAME}
                        ${NEO_SHARED_MOCKABLE_LIB_NAME}
                        gmock-gtest
                        ${NEO_EXTRA_LIBS}
  )

  set_target_properties(neo_shared_mt_tests PROPERTIES
                        FOLDER ${SHARED_TEST_PROJECTS_FOLDER}
  )

  set_property(TARGET neo_shared_mt_tests PROPERTY ENABLE_EXPORTS TRUE)
  set_property(TARGET neo_shared_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${TSAN_FLAGS})
  if(NOT WIN32)
    set_property(TARGET neo_shared_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
  endif()

  set_target_properties(run_shared_mt_unit_tests PROPERTIES
                        FOLDER ${SHARED_TEST_PROJECTS_FOLDER}
  )

  create_project_source_tree(neo_shared_mt_tests)
endif()
