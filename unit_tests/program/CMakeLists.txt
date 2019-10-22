#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_program
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/block_kernel_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/evaluate_unhandled_token_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_data.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_data_OCL2_0.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/printf_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/process_debug_data_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/process_elf_binary_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/process_spir_binary_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_data_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_from_binary.h
  ${CMAKE_CURRENT_SOURCE_DIR}/program_nonuniform.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_spec_constants_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/program_with_block_kernels_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_with_kernel_debug_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_with_source.h
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_program})
add_subdirectories()
