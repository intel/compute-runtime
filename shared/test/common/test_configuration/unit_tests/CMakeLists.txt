#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_unit_tests_configurations
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_resource_info_common.cpp
)
if(WIN32)
  list(APPEND NEO_SHARED_unit_tests_configurations
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory${BRANCH_DIR_SUFFIX}mock_gmm_memory.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/mock_gmm_memory_base.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/mock_gmm_memory_base.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/os_memory_virtual_alloc_ult.cpp
  )
elseif(UNIX AND NOT DISABLE_WDDM_LINUX)
  list(APPEND NEO_SHARED_unit_tests_configurations
       ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
  )
endif()
if(DEFINED AUB_STREAM_PROJECT_NAME)
  list(APPEND NEO_SHARED_unit_tests_configurations
       ${NEO_SHARED_TEST_DIRECTORY}/common/aub_stream_mocks/aub_stream_interface_mock.cpp
  )
else()
  list(APPEND NEO_SHARED_unit_tests_configurations
       ${NEO_SOURCE_DIR}/shared/source/aub/aub_stream_interface.cpp
  )
endif()
add_library(neo_unit_tests_config OBJECT EXCLUDE_FROM_ALL ${NEO_SHARED_unit_tests_configurations})

set_target_properties(neo_unit_tests_config PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(neo_unit_tests_config PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
set_property(TARGET neo_unit_tests_config APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(neo_unit_tests_config PRIVATE
                           $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)
if(WIN32)
  target_include_directories(neo_unit_tests_config PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory${BRANCH_DIR_SUFFIX}
  )
endif()
target_compile_definitions(neo_unit_tests_config PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
create_project_source_tree(neo_unit_tests_config)
