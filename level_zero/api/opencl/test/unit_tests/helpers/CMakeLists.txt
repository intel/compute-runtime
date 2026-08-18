#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_base_object.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_to_l0_handles.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_validators.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_convert_color.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_error_mappers.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_get_info_status_mapper.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_gmm_types_converter.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_image_enqueue_helpers.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_to_cl_return_types_mapper.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_surface_formats.cpp
)
