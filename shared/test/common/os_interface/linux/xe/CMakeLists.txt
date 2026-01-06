#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(neo_libult_common_SRCS_LIB_ULT_LINUX_XE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_drm_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_drm_xe.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_drm_xe.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_drm_xe_definitions.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/xe_config_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_drm_xe_perf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_drm_xe_perf.h
)
if(UNIX)

  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_LINUX_XE})
endif()
add_subdirectories()

set_property(GLOBAL APPEND PROPERTY neo_libult_common_SRCS_LIB_ULT_LINUX_XE ${neo_libult_common_SRCS_LIB_ULT_LINUX_XE})

