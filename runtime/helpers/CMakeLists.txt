#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_HELPERS_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/abort.h
  ${CMAKE_CURRENT_SOURCE_DIR}/address_patch.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aligned_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/array_count.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/basic_math.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/completion_stamp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/convert_color.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/device_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enable_product.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/error_mappers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/extendable_enum.h
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper_hw.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/get_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hash.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_common.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_commands.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_commands.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_commands_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_properties.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_properties.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mipmap.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mipmap.h
  ${CMAKE_CURRENT_SOURCE_DIR}/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/options.h
  ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_select_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/queue_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address.h
  ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/stdio.h
  ${CMAKE_CURRENT_SOURCE_DIR}/string.h
  ${CMAKE_CURRENT_SOURCE_DIR}/string_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.h
  ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet.h
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information.h
  ${CMAKE_CURRENT_SOURCE_DIR}/uint16_avx2.h
  ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/validators.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/validators.h
)
set(RUNTIME_SRCS_HELPERS_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/kmd_notify_properties_windows.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_helper.h
)
set(RUNTIME_SRCS_HELPERS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/kmd_notify_properties_linux.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_HELPERS_BASE})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_HELPERS_WINDOWS})
else()
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_HELPERS_LINUX})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_HELPERS_LINUX ${RUNTIME_SRCS_HELPERS_LINUX})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_HELPERS_BASE ${RUNTIME_SRCS_HELPERS_BASE})

add_subdirectories()
