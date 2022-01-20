#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}drm_query_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_special_heap_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_uuid_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_l0_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_TESTS_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_LINUX})

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_OS_INTERFACE_TESTS_LINUX}
  )
endif()
add_subdirectories()
