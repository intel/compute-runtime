#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_AUB_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler_tests.h
)

set(NEO_CORE_OS_INTERFACE_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/device_uuid_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_TESTS ${NEO_CORE_OS_INTERFACE_TESTS})

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_AUB_TESTS ${NEO_CORE_OS_INTERFACE_AUB_TESTS})

target_sources(${TARGET_NAME} PRIVATE
               ${NEO_CORE_OS_INTERFACE_TESTS}
               ${NEO_CORE_OS_INTERFACE_AUB_TESTS}
)

add_subdirectories()
