#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config_ocl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_hw_parse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_memory_properties_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info_status_mapper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner_ocl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mipmap_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/task_information_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_properties_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/validator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_helper_ocl_tests.cpp
)

if(TESTS_XEHP_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_helpers
       ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_tests_xehp_and_later.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xehp_and_later.cpp
  )
endif()

if(TESTS_DG2_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_helpers
       ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_dg2_and_later.cpp
  )
endif()
if(TESTS_XE3_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_helpers
       ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xe3_and_later.cpp
  )
endif()

get_property(NEO_CORE_PREAMBLE_TESTS GLOBAL PROPERTY NEO_CORE_PREAMBLE_TESTS)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers})
add_subdirectories()
