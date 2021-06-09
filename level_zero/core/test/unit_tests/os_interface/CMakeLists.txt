#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_L0_OS_INTERFACE_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_l0_tests.cpp
)

add_subdirectories()

target_sources(${TARGET_NAME} PRIVATE
               ${NEO_L0_OS_INTERFACE_TESTS}
)
