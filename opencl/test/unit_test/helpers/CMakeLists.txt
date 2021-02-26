#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config_ocl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info_status_mapper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner_ocl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_default_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_filename_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_management_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mipmap_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raii_hw_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ray_tracing_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/task_information_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_properties_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_limits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/validator_tests.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/aligned_memory_tests.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/debug_manager_state_restore.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper.inl
)

get_property(NEO_CORE_PREAMBLE_TESTS GLOBAL PROPERTY NEO_CORE_PREAMBLE_TESTS)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers})
add_subdirectories()
