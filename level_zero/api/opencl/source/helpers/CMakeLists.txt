#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_HELPERS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_base_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}leo_cl_memory_properties_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_memory_properties_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_memory_properties_helpers_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_to_l0_handles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_to_l0_handles.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_validators.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_validators.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_error_mappers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_get_info_status_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gmm_types_converter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gmm_types_converter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image_enqueue_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/l0_to_cl_return_types_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_properties_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_surface_formats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_surface_formats.h
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_HELPERS_SRCS})

set_property(GLOBAL PROPERTY LEO_HELPERS_SRCS ${LEO_HELPERS_SRCS})

add_subdirectories()
