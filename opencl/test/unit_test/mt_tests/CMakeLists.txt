#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT NEO_SKIP_MT_UNIT_TESTS)
  project(igdrcl_mt_tests)

  set(OPENCL_MT_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  add_custom_target(run_mt_unit_tests)
  add_executable(igdrcl_mt_tests EXCLUDE_FROM_ALL
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/test_macros/test_checks_ocl.cpp
                 $<TARGET_OBJECTS:igdrcl_libult>
                 $<TARGET_OBJECTS:neo_libult_common>
                 $<TARGET_OBJECTS:neo_libult_cs>
                 $<TARGET_OBJECTS:neo_libult>
                 $<TARGET_OBJECTS:neo_shared_mocks>
                 $<TARGET_OBJECTS:neo_mt_tests_config>
                 $<TARGET_OBJECTS:igdrcl_libult_env>
                 $<TARGET_OBJECTS:mock_aubstream>
                 $<TARGET_OBJECTS:mock_gmm>
                 $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  )

  target_include_directories(igdrcl_mt_tests PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/mt_tests
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                             ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/includes${BRANCH_DIR_SUFFIX}
                             ${NEO_SOURCE_DIR}/opencl/source/gen_common
  )

  add_subdirectories()

  target_link_libraries(igdrcl_mt_tests ${TSAN_LIB})
  target_link_libraries(igdrcl_mt_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})
  target_link_libraries(igdrcl_mt_tests gmock-gtest)
  target_link_libraries(igdrcl_mt_tests igdrcl_mocks ${NEO_EXTRA_LIBS})

  add_dependencies(igdrcl_mt_tests
                   prepare_test_kernels_for_shared
                   prepare_test_kernels_for_ocl
  )
  create_project_source_tree(igdrcl_mt_tests)

  set_target_properties(igdrcl_mt_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
  set_property(TARGET igdrcl_mt_tests PROPERTY ENABLE_EXPORTS TRUE)
  set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${TSAN_FLAGS})
  if(NOT WIN32)
    set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
  endif()

  set_target_properties(run_mt_unit_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
endif()
