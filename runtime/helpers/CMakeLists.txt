#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_HELPERS_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_blit_properties.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/cl_device_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/convert_color.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enable_product.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_properties.h
  ${CMAKE_CURRENT_SOURCE_DIR}/error_mappers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/get_info_status_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware_context_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware_context_controller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/helper_options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mem_properties_parser_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_properties_parser_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/memory_properties_flags_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_flags_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_flags_helpers_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/mipmap.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mipmap.h
  ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/queue_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/string_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.h
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information.h
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/uint16_avx2.h
  ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/validators.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/validators.h
)

set(RUNTIME_SRCS_HELPERS_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_callbacks_tgllp_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/kmd_notify_properties_windows.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/gl_helper.h
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
