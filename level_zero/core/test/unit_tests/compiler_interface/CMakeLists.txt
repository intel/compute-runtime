#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_L0_COMPILER_INTERFACE_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

target_sources(${TARGET_NAME} PRIVATE
               ${NEO_L0_COMPILER_INTERFACE_TESTS}
)

if(NOT WIN32)
  target_sources(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/linux/default_l0_cache_config_tests.cpp)
else()
  target_sources(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/windows/default_l0_cache_config_tests.cpp)
endif()

