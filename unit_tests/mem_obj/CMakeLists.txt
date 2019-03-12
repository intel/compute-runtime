#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_mem_obj
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer_pin_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer_set_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_image_format_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/destructor_callback_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_mem_object_info_subbuffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_mem_object_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_from_subbuffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image1d_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image2d_from_buffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image2d_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image3d_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_array_size_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_compression_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/image_format_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_redescribe_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_release_mapped_ptr_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_set_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_snorm_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_tiled_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_transfer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_validate_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/map_operations_handler_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj_destruction_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/nv12_image_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/packed_yuv_image_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pipe_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sub_buffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/zero_copy_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_mem_obj})
add_subdirectories()
