#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_kernels_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_hw_command_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_indirect_data_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_kernel_group_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_load_register_imm_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_pipe_control_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_semaphore_wait_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_store_data_imm_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_store_register_mem_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/variable_tests.cpp
)

if(TESTS_XE3_AND_BEFORE)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_hw_command_tests_from_xe_to_xe3.cpp
  )
endif()

add_subdirectories()
