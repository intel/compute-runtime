#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_mt_tests)

set(OPENCL_MT_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

option(SHOW_VERBOSE_UTESTS_RESULTS "Use the default/verbose test output" ON)

if(NOT SHOW_VERBOSE_UTESTS_RESULTS)
  set(igdrcl_mt_tests_LISTENER_OPTION "--disable_default_listener")
endif()

add_custom_target(run_mt_unit_tests)
add_executable(igdrcl_mt_tests EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/os_interface.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_configuration.cpp
  ${NEO_SOURCE_DIR}/opencl/source/aub/aub_stream_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
  $<TARGET_OBJECTS:mock_gmm>
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
)

target_include_directories(igdrcl_mt_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/test_macros${BRANCH_DIR_SUFFIX}
  ${NEO_SOURCE_DIR}/opencl/source/gen_common
)

add_subdirectories()

target_link_libraries(igdrcl_mt_tests ${TSAN_LIB})
target_link_libraries(igdrcl_mt_tests ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE})
target_link_libraries(igdrcl_mt_tests gmock-gtest)
target_link_libraries(igdrcl_mt_tests igdrcl_mocks ${IGDRCL_EXTRA_LIBS})

if(WIN32)
  target_sources(igdrcl_mt_tests PRIVATE
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_create.cpp
  )
else()
  target_sources(igdrcl_mt_tests PRIVATE
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/drm_neo_create.cpp
  )
endif()

if(WIN32)
  add_dependencies(igdrcl_mt_tests mock_gdi igdrcl_tests)
endif()


add_dependencies(igdrcl_mt_tests test_dynamic_lib)

create_project_source_tree(igdrcl_mt_tests ${NEO_SOURCE_DIR}/runtime ${NEO_SOURCE_DIR}/unit_tests)

set_target_properties(igdrcl_mt_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${TSAN_FLAGS})
if(NOT WIN32)
  set_property(TARGET igdrcl_mt_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()

set_target_properties(run_mt_unit_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})

