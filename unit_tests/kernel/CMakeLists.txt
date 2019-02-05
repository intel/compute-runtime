#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_kernel
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/clone_kernel_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_transformer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_accelerator_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_buffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_buffer_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_buffer_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_pipe_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_svm_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_image_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_immediate_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_is_patched_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_dev_queue_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_reflection_surface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_slm_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_slm_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_transformable_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_kernel_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/parent_kernel_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/substitute_kernel_heap_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_kernel})
add_subdirectories()
