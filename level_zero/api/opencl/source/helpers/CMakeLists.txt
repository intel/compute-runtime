#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_HELPERS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/base_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cl_memory_properties_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_memory_properties_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_memory_properties_helpers_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_to_l0_handles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_to_l0_handles.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_validators.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_validators.h
    ${CMAKE_CURRENT_SOURCE_DIR}/error_mappers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info_status_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_types_converter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gmm_types_converter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/l0_to_cl_return_types_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/properties_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/surface_formats.h
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_HELPERS_SRCS})

set_property(GLOBAL PROPERTY LEO_HELPERS_SRCS ${LEO_HELPERS_SRCS})

add_subdirectories()
