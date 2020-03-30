#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(RUNTIME_SRCS_SHARINGS_GL_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_arb_sync_event_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_buffer_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_context_guard_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_library_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing_windows.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_sync_event_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_texture_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gl_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/win_enable_gl.h
  )
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_GL_WINDOWS})

  set(RUNTIME_SRCS_SHARINGS_GL_ENABLE_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/win_enable_gl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/win_enable_gl.h
  )
  target_sources(${SHARINGS_ENABLE_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_GL_ENABLE_WINDOWS})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SHARINGS_GL_WINDOWS ${RUNTIME_SRCS_SHARINGS_GL_WINDOWS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SHARINGS_GL_ENABLE_WINDOWS ${RUNTIME_SRCS_SHARINGS_GL_ENABLE_WINDOWS})
