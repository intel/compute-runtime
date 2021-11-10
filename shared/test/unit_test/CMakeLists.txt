#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_TEST_PROJECTS_FOLDER "neo shared")
if(NOT NEO_SKIP_UNIT_TESTS)

  add_custom_target(unit_tests)

  set(TARGET_NAME neo_shared_tests)

  include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

  function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
    set(NEO_SUPPORTED_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
    string(REPLACE ";" "," NEO_SUPPORTED_PRODUCT_FAMILIES "${NEO_SUPPORTED_PRODUCT_FAMILIES}")
    add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${NEO_SUPPORTED_PRODUCT_FAMILIES})
  endfunction()

  ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()
  link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

  add_executable(${TARGET_NAME}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/ult_specific_config.cpp
                 ${NEO_SHARED_DIRECTORY}/helpers/allow_deferred_deleter.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/api_specific_config_shared_tests.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_checks_shared.cpp
                 $<TARGET_OBJECTS:mock_gmm>
                 $<TARGET_OBJECTS:neo_libult_common>
                 $<TARGET_OBJECTS:neo_libult_cs>
                 $<TARGET_OBJECTS:neo_libult>
                 $<TARGET_OBJECTS:neo_shared_mocks>
  )

  set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}")

  target_include_directories(${TARGET_NAME} PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
                             ${ENGINE_NODE_DIR}
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                             ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/includes${BRANCH_DIR_SUFFIX}
  )

  if(WIN32)
    target_link_libraries(${TARGET_NAME} dbghelp)
  endif()

  target_link_libraries(${TARGET_NAME}
                        gmock-gtest
                        ${NEO_SHARED_MOCKABLE_LIB_NAME}
                        ${NEO_EXTRA_LIBS}
  )

  if(MSVC)
    set_target_properties(${TARGET_NAME} PROPERTIES
                          VS_DEBUGGER_COMMAND_ARGUMENTS "--gtest_filter=* --gtest_catch_exceptions=0 --enable_default_listener --disable_pagefaulting_tests"
                          VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )
  endif()

  add_subdirectory(${NEO_SHARED_TEST_DIRECTORY}/common "${NEO_BUILD_DIR}/shared/test/common")
  add_subdirectory_unique(mocks)

  if(NOT NEO_SKIP_SHARED_UNIT_TESTS)
    add_subdirectories()
  endif()

  target_sources(${TARGET_NAME} PRIVATE $<TARGET_OBJECTS:neo_unit_tests_config>)
  add_dependencies(unit_tests prepare_test_kernels_for_shared)
  add_dependencies(unit_tests ${TARGET_NAME})

  create_project_source_tree(${TARGET_NAME})
endif()
