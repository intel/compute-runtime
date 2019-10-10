#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_executable(igdrcl_tbx_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/main_tbx.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_tests_configuration.cpp
  ${NEO_SOURCE_DIR}/runtime/dll/create_command_stream.cpp
  ${NEO_SOURCE_DIR}/runtime/dll${BRANCH_DIR_SUFFIX}/get_devices.cpp
  ${NEO_SOURCE_DIR}/unit_tests/aub_stream_mocks/aub_stream_interface_mock.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/os_interface.cpp
  ${NEO_SOURCE_DIR}/unit_tests/options.cpp
  $<TARGET_OBJECTS:igdrcl_libult>
)

target_link_libraries(igdrcl_tbx_tests ${NEO_MOCKABLE_LIB_NAME} igdrcl_mocks gmock-gtest ${IGDRCL_EXTRA_LIBS})

if(WIN32)
  target_include_directories(igdrcl_tbx_tests PRIVATE
    ${NEO_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
  )
  target_include_directories(igdrcl_tbx_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(igdrcl_tbx_tests PRIVATE
    ${NEO_SOURCE_DIR}/runtime/dll/windows/options.cpp
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/create_wddm_memory_manager.cpp
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/sys_calls.cpp
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_calls.cpp
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
  )
  target_link_libraries(igdrcl_tbx_tests dbghelp)
else()
  target_sources(igdrcl_tbx_tests PRIVATE
    ${NEO_SOURCE_DIR}/runtime/dll/linux/allocator_helper.cpp
    ${NEO_SOURCE_DIR}/runtime/dll/linux/options.cpp
    ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/create_drm_memory_manager.cpp
  )
endif()

copy_gmm_dll_for(igdrcl_tbx_tests)
create_project_source_tree(igdrcl_tbx_tests ${NEO_SOURCE_DIR}/runtime ${NEO_SOURCE_DIR}/unit_tests)

add_custom_target(run_tbx_tests ALL DEPENDS unit_tests igdrcl_tbx_tests)
add_custom_command(
  TARGET run_tbx_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo Target Directory is: ${TargetDir}
  COMMAND echo Running TBX tests
  COMMAND igdrcl_tbx_tests
)
add_dependencies(run_unit_tests run_tbx_tests)

set_target_properties(igdrcl_tbx_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
set_target_properties(run_tbx_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
