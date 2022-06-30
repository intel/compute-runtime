#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/command_enqueue_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer_rect_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_image_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_fill_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_fill_image_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_map_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_map_image_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_printf_kernel_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_buffer_rect_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_image_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_verify_memory_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_verify_memory_image_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_buffer_rect_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_copy_read_buffer_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_copy_read_buffer_aub_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_image_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/single_tile_products_excludes.cpp
)

if(TESTS_XEHP_AND_LATER)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_enqueue_resource_barrier_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_inline_data_local_id_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_multicontext_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_one_va_multi_physical_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_postsync_write_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_scratch_space_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compression_aub_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/multi_tile_buffers_aub_tests_xehp_and_later.cpp
  )
endif()

add_subdirectories()
