#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT NEO_SKIP_AUB_TESTS AND DEFINED AUB_STREAM_PROJECT_NAME)
  project(igdrcl_aub_tests)

  set(OPENCL_AUB_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS
       ${NEO_SOURCE_DIR}/opencl/test/unit_test/test_macros/test_checks_ocl.cpp
       $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
       $<TARGET_OBJECTS:igdrcl_libult>
       $<TARGET_OBJECTS:neo_libult_common>
       $<TARGET_OBJECTS:neo_libult_cs>
       $<TARGET_OBJECTS:neo_libult>
       $<TARGET_OBJECTS:igdrcl_libult_env>
       $<TARGET_OBJECTS:neo_shared_mocks>
       $<TARGET_OBJECTS:neo_aub_tests_config>
  )

  list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>)

  add_executable(igdrcl_aub_tests
                 ${IGDRCL_AUB_TESTS__TARGET_OBJECTS}
  )

  set_target_properties(igdrcl_aub_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
  set_property(TARGET igdrcl_aub_tests PROPERTY ENABLE_EXPORTS TRUE)

  target_sources(igdrcl_aub_tests PRIVATE
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDLESS_LIB_NAME}>
  )

  if(MSVC)
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

  copy_gmm_dll_for(igdrcl_aub_tests)
  add_subdirectories()

  target_link_libraries(igdrcl_aub_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})
  target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
  target_link_libraries(igdrcl_aub_tests gmock-gtest ${NEO_EXTRA_LIBS})
  target_link_libraries(igdrcl_aub_tests ${AUB_STREAM_PROJECT_NAME})

  if(UNIX)
    target_link_libraries(igdrcl_aub_tests ${GMM_LINK_NAME})
  else()
    add_dependencies(igdrcl_aub_tests ${GMM_TARGET_NAME})
  endif()
  target_include_directories(igdrcl_aub_tests BEFORE PRIVATE ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX})

  create_project_source_tree(igdrcl_aub_tests)
endif()
