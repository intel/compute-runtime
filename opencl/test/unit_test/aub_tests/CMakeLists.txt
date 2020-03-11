#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_aub_tests)

set(OPENCL_AUB_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.cpp
  ${NEO_SOURCE_DIR}/opencl/source/aub/aub_stream_interface.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/os_interface.cpp
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
)

if(DEFINED AUB_STREAM_DIR)
  list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>)
endif()

add_executable(igdrcl_aub_tests
  ${IGDRCL_AUB_TESTS__TARGET_OBJECTS}
)

set_target_properties(igdrcl_aub_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})

if(WIN32)
  set_target_properties(igdrcl_aub_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${TargetDir}
  )
endif()

add_custom_target(run_aub_tests)
set_target_properties(run_aub_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})

if(WIN32)
  target_include_directories(igdrcl_aub_tests PRIVATE
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks${BRANCH_DIR_SUFFIX}
  )
endif()

target_include_directories(igdrcl_aub_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(igdrcl_aub_tests PRIVATE
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/page_fault_manager/default_asan_options.cpp
  ${NEO_SHARED_DIRECTORY}/gmm_helper/resource_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
)

if(WIN32)
  target_sources(igdrcl_aub_tests PRIVATE
    ${NEO_SHARED_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
    ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_create.cpp
  )
else()
  target_sources(igdrcl_aub_tests PRIVATE
    ${NEO_SHARED_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/drm_neo_create.cpp
  )
endif()

copy_gmm_dll_for(igdrcl_aub_tests)
add_subdirectories()

target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE})
target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests gmock-gtest ${IGDRCL_EXTRA_LIBS})
if(UNIX)
  target_link_libraries(igdrcl_aub_tests ${GMM_LINK_NAME})
else()
  add_dependencies(igdrcl_aub_tests ${GMM_TARGET_NAME})
  target_sources(igdrcl_aub_tests PRIVATE
    ${NEO_SHARED_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
  )
endif()
target_include_directories(igdrcl_aub_tests BEFORE PRIVATE ${NEO_SHARED_TEST_DIRECTORY}/unit_test/test_macros${BRANCH_DIR_SUFFIX})

create_project_source_tree(igdrcl_aub_tests)
