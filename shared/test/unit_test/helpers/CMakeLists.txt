#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_SRCS_tests_helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/app_resource_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/array_count_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic_math_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bit_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_hw_info_config_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extendable_enum_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_gpgpu_engines_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/l3_range_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_management_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raii_hw_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/static_size3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_variables.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_limits.h
)

if(MSVC OR COMPILER_SUPPORTS_SSE42)
  list(APPEND SHARED_SRCS_tests_helpers
       ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4_tests.cpp
  )
endif()

if(COMPILER_SUPPORTS_NEON)
  list(APPEND SHARED_SRCS_tests_helpers
       ${CMAKE_CURRENT_SOURCE_DIR}/uint16_neon_tests.cpp
  )
endif()

target_sources(${TARGET_NAME} PRIVATE ${SHARED_SRCS_tests_helpers})
add_subdirectories()
