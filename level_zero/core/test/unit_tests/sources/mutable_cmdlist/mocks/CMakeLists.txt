#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_cmdlist.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_indirect_data.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_kernel.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_kernel_group.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_load_register_imm_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_pipe_control_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_semaphore_wait_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_store_data_imm_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_mutable_store_register_mem_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_variable.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_variable_dispatch.h
)

add_subdirectories()
