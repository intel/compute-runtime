#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_COMMAND_QUEUE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_local_work_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_local_work_size.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_staging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_state.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_data_transfer_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_selection_args.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_selection_args.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_barrier.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer_rect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer_to_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_image_to_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_fill_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_fill_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_marker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_migrate_mem_objects.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_buffer_rect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_resource_barrier.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_svm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_buffer_rect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/finish.h
    ${CMAKE_CURRENT_SOURCE_DIR}/flush.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gpgpu_walker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gpgpu_walker_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}hardware_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_interface_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_barrier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_barrier.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND RUNTIME_SRCS_COMMAND_QUEUE
       ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_hw_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/gpgpu_walker_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/hardware_interface_xehp_and_later.inl
  )
endif()

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_COMMAND_QUEUE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_COMMAND_QUEUE ${RUNTIME_SRCS_COMMAND_QUEUE})
add_subdirectories()
