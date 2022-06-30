#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_flush_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/clone_kernel_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_transformer_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_accelerator_arg_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_buffer_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_buffer_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_buffer_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_pipe_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_svm_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_cache_flush_requirements_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_info_cl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_image_arg_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_immediate_arg_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_is_patched_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_slm_arg_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_slm_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_transformable_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_kernel_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/substitute_kernel_heap_tests.cpp
)

if(TESTS_XEHP_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_kernel
       ${CMAKE_CURRENT_SOURCE_DIR}/cache_flush_xehp_and_later_tests.inl
  )
endif()

if(TESTS_PVC_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_kernel
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_pvc_and_later.cpp
  )
endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_kernel})
add_subdirectories()
