#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_HELPERS_BASE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/base_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_blit_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_factory_init.inl
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cl_memory_properties_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_memory_properties_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_memory_properties_helpers_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_preemption_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_preemption_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_validators.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_validators.h
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_color.h
    ${CMAKE_CURRENT_SOURCE_DIR}/destructor_callbacks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/error_mappers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info_status_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_types_converter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_types_converter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/helper_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_ocl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mipmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mipmap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.h
    ${CMAKE_CURRENT_SOURCE_DIR}/task_information.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/task_information.h
    ${CMAKE_CURRENT_SOURCE_DIR}/task_information.inl
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND RUNTIME_SRCS_HELPERS_BASE
       ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_xehp_and_later.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND RUNTIME_SRCS_HELPERS_BASE ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_pvc_and_later.inl)
endif()

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_HELPERS_BASE})

set_property(GLOBAL PROPERTY RUNTIME_SRCS_HELPERS_BASE ${RUNTIME_SRCS_HELPERS_BASE})

add_subdirectories()
