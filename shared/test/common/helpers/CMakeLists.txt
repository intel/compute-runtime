#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/bindless_heaps_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager_state_restore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/default_hw_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/default_hw_info.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_flags_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_descriptor_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_io_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hash_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}hw_helper_extended_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_binary_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_filename_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_listener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_management.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/string_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string_to_hash_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_traits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_platforms.h
    ${CMAKE_CURRENT_SOURCE_DIR}/includes${BRANCH_DIR_SUFFIX}test_traits_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/includes${BRANCH_DIR_SUFFIX}test_traits_platforms_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_hw_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_hw_config.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_hw_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/variable_backup.h
)

if(TESTS_XEHP_AND_LATER)
  list(APPEND NEO_CORE_HELPERS_TESTS
       ${CMAKE_CURRENT_SOURCE_DIR}/test_blit_commands_helper_xehp_and_later.cpp
  )
endif()

if(TESTS_DG2_AND_LATER)
  list(APPEND NEO_CORE_HELPERS_TESTS
       ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_dg2_and_later.cpp
  )
endif()

if(TESTS_PVC_AND_LATER)
  list(APPEND NEO_CORE_HELPERS_TESTS
       ${CMAKE_CURRENT_SOURCE_DIR}/test_blit_commands_helper_pvc_and_later.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_pvc_and_later.inl
  )
endif()

add_subdirectories()

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner_shared_tests.cpp
               ${NEO_CORE_HELPERS_TESTS}
)
