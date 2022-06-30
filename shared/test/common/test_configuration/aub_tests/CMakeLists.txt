#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_aub_tests_configurations
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
    ${NEO_SOURCE_DIR}/shared/source/aub/aub_stream_interface.cpp
    ${NEO_SHARED_DIRECTORY}/gmm_helper/resource_info.cpp
)

if(UNIX)
  list(APPEND NEO_SHARED_aub_tests_configurations
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
  )
else()
  list(APPEND NEO_SHARED_aub_tests_configurations
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_memory_virtual_alloc.cpp
  )
endif()

if(WIN32 OR(UNIX AND NOT DISABLE_WDDM_LINUX))
  list(APPEND NEO_SHARED_aub_tests_configurations
       ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
  )
endif()

add_subdirectories()
add_library(neo_aub_tests_config OBJECT EXCLUDE_FROM_ALL ${NEO_SHARED_aub_tests_configurations})

set_target_properties(neo_aub_tests_config PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(neo_aub_tests_config PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
set_property(TARGET neo_aub_tests_config APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(neo_aub_tests_config PUBLIC
                           $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(neo_aub_tests_config PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
create_project_source_tree(neo_aub_tests_config)
