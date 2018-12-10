#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_executable(igdrcl_tbx_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/main_tbx.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_tests_configuration.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/aub/aub_stream_interface.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/dll/create_command_stream.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/options.cpp
  $<TARGET_OBJECTS:igdrcl_libult>
)

target_link_libraries(igdrcl_tbx_tests ${NEO_MOCKABLE_LIB_NAME} igdrcl_mocks gmock-gtest ${IGDRCL_EXTRA_LIBS})

if(WIN32)
  target_include_directories(igdrcl_tbx_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
  )
  target_sources(igdrcl_tbx_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/runtime/dll/windows/options.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/sys_calls.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_calls.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
  )
else()
  target_sources(igdrcl_tbx_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/allocator_helper.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/options.cpp
  )
endif()

copy_gmm_dll_for(igdrcl_tbx_tests)
create_project_source_tree(igdrcl_tbx_tests ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)

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
