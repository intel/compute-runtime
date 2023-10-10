#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_mt_tests_configurations
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mt_test_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_resource_info_common.cpp
)
if(WIN32)
  list(APPEND NEO_SHARED_mt_tests_configurations
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory${BRANCH_DIR_SUFFIX}mock_gmm_memory.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/mock_gmm_memory_base.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/mock_gmm_memory_base.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/os_memory_virtual_alloc_ult.cpp
  )
elseif(UNIX AND NOT DISABLE_WDDM_LINUX)
  list(APPEND NEO_SHARED_mt_tests_configurations
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory/mock_gmm_memory.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/mock_gmm_memory_base.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/mock_gmm_memory_base.h
  )
endif()
list(APPEND NEO_SHARED_mt_tests_configurations
     ${NEO_SHARED_TEST_DIRECTORY}/common/aub_stream_mocks/aub_stream_interface_mock.cpp
)
add_library(neo_mt_tests_config OBJECT EXCLUDE_FROM_ALL ${NEO_SHARED_mt_tests_configurations})

set_target_properties(neo_mt_tests_config PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(neo_mt_tests_config PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
set_property(TARGET neo_mt_tests_config APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(neo_mt_tests_config PRIVATE
                           $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)
if(WIN32)
  target_include_directories(neo_mt_tests_config PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory${BRANCH_DIR_SUFFIX}
  )
elseif(NOT DISABLE_WDDM_LINUX)
  target_include_directories(neo_mt_tests_config PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/windows/gmm_memory
  )
endif()
target_compile_definitions(neo_mt_tests_config PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
create_project_source_tree(neo_mt_tests_config)
