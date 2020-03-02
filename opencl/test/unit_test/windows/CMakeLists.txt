#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
project(igdrcl_windows_dll_tests)

set(NEO_IGDRCL_WINDOWS_DLL_TESTS_TARGET_OBJECTS
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_env>
  $<TARGET_OBJECTS:mock_gmm>
)

add_executable(igdrcl_windows_dll_tests
  ${NEO_IGDRCL_WINDOWS_DLL_TESTS_TARGET_OBJECTS}
  ${NEO_SOURCE_DIR}/opencl/source/aub/aub_stream_interface.cpp
  ${NEO_SOURCE_DIR}/opencl/source/dll/create_command_stream.cpp
  ${NEO_SOURCE_DIR}/opencl/source/dll${BRANCH_DIR_SUFFIX}/get_devices.cpp
  ${NEO_SOURCE_DIR}/opencl/source/dll/windows/os_interface.cpp
  ${NEO_CORE_DIRECTORY}/os_interface/windows/wddm/wddm_create.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/get_devices_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_create_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
)

target_link_libraries(igdrcl_windows_dll_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_CORE_MOCKABLE_LIB_NAME} igdrcl_mocks gmock-gtest ${IGDRCL_EXTRA_LIBS})

target_include_directories(igdrcl_windows_dll_tests PRIVATE
  ${NEO_CORE_TEST_DIRECTORY}/unit_test/test_macros${BRANCH_DIR_SUFFIX}
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/gen_common${BRANCH_DIR_SUFFIX}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

create_project_source_tree(igdrcl_windows_dll_tests ${NEO_SOURCE_DIR}/runtime ${NEO_SOURCE_DIR}/unit_tests)

add_custom_target(run_windows_dll_tests ALL DEPENDS unit_tests igdrcl_windows_dll_tests)
add_custom_command(
  TARGET run_windows_dll_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo Target Directory is: ${TargetDir}
  COMMAND echo Running Windows dll tests
  COMMAND igdrcl_windows_dll_tests
)
add_dependencies(run_unit_tests run_windows_dll_tests)

set_target_properties(igdrcl_windows_dll_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_target_properties(run_windows_dll_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
endif()
