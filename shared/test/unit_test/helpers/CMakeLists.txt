#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aligned_memory_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/app_resource_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/array_count_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/basic_math_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/bindless_heaps_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/bit_helpers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_tests_gen12lp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/compiler_hw_info_config_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_helpers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/extendable_enum_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/file_io_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/get_gpgpu_engines_tests.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/get_info_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/hash_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner_shared_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/l3_range_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/matcher_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_management_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/string_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/string_to_hash_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_variables.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_tests.cpp
)

if(MSVC OR COMPILER_SUPPORTS_SSE42)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4_tests.cpp)
endif()

if(COMPILER_SUPPORTS_NEON)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/uint16_neon_tests.cpp)
endif()

if(TESTS_XEHP_AND_LATER)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_blit_commands_helper_xehp_and_later.cpp)
endif()

if(TESTS_DG2_AND_LATER)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_dg2_and_later.cpp)
endif()

if(TESTS_PVC_AND_LATER)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_blit_commands_helper_pvc_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_pvc_and_later.inl
  )
endif()

add_subdirectories()