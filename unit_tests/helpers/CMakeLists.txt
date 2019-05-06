#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_helpers
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/array_count_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aligned_memory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests_mt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/basic_math_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/bit_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager_state_restore.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/extendable_enum_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_gpgpu_engines_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/get_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gtest_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hash_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_default_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_commands_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_commands_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_filename_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_management_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mipmap_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raii_hw_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/string_to_hash_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/string_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_variables.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/transfer_properties_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/validator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/variable_backup.h
)

set(IGDRCL_SRCS_tests_helpers_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/kmd_notify_windows_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/gl_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_function.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_function.cpp
)

set(IGDRCL_SRCS_tests_helpers_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/kmd_notify_linux_tests.cpp
)

if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers_windows})
 else()
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers_linux})
 endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_helpers_linux ${IGDRCL_SRCS_tests_helpers_linux})

add_subdirectories()
