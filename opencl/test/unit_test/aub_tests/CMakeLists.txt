#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_aub_tests)

set(OPENCL_AUB_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS
     ${NEO_SOURCE_DIR}/opencl/test/unit_test/test_macros/test_checks_ocl.cpp
     ${NEO_SOURCE_DIR}/shared/test/common/test_macros/test_checks_shared.cpp
     $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
     $<TARGET_OBJECTS:igdrcl_libult>
     $<TARGET_OBJECTS:neo_libult_common>
     $<TARGET_OBJECTS:neo_libult_cs>
     $<TARGET_OBJECTS:neo_libult>
     $<TARGET_OBJECTS:igdrcl_libult_env>
     $<TARGET_OBJECTS:neo_shared_mocks>
     $<TARGET_OBJECTS:neo_aub_tests_config>
)

if(DEFINED AUB_STREAM_PROJECT_NAME)
  list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>)
endif()

add_executable(igdrcl_aub_tests
               ${IGDRCL_AUB_TESTS__TARGET_OBJECTS}
)

set_target_properties(igdrcl_aub_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})

if(WIN32)
  set_target_properties(igdrcl_aub_tests PROPERTIES
                        VS_DEBUGGER_WORKING_DIRECTORY ${TargetDir}
                        VS_DEBUGGER_COMMAND_ARGUMENTS " --disable_pagefaulting_tests"
  )
endif()

if(WIN32)
  target_include_directories(igdrcl_aub_tests PRIVATE
                             ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks${BRANCH_DIR_SUFFIX}
  )
endif()

target_include_directories(igdrcl_aub_tests PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/aub_tests
                           ${NEO_SOURCE_DIR}/shared/test/common/helpers/includes${BRANCH_DIR_SUFFIX}
)

target_sources(igdrcl_aub_tests PRIVATE
               ${NEO_SHARED_TEST_DIRECTORY}/unit_test/page_fault_manager/default_asan_options.cpp
)
copy_gmm_dll_for(igdrcl_aub_tests)
add_subdirectories()

target_link_libraries(igdrcl_aub_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests gmock-gtest ${NEO_EXTRA_LIBS})
if(UNIX)
  target_link_libraries(igdrcl_aub_tests ${GMM_LINK_NAME})
else()
  add_dependencies(igdrcl_aub_tests ${GMM_TARGET_NAME})
endif()
target_include_directories(igdrcl_aub_tests BEFORE PRIVATE ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX})

create_project_source_tree(igdrcl_aub_tests)
