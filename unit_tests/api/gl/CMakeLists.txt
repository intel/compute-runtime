#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_tests_api_gl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_from_gl_buffer_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_from_gl_renderbuffer_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_from_gl_texture2d_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_from_gl_texture3d_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_from_gl_texture_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_enqueue_acquire_gl_objects_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_enqueue_release_gl_objects_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_gl_device_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_gl_context_info_khr_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_gl_object_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_gl_texture_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_intel_tracing_tests.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_api_gl})
endif()
