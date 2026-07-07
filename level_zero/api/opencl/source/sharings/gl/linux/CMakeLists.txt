#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)

  set(LEO_SHARINGS_GL_LINUX_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_gl_api_helper.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/leo_gl_types.h
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_lin_enable_gl.h
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_arb_sync_event_linux.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_buffer_linux.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_context_guard_linux.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_sharing_linux.h
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_sharing_linux.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_sync_event_linux.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_texture_linux.cpp
  )

  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_GL_LINUX_SRCS})

  set(LEO_SHARINGS_GL_ENABLE_LINUX_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_lin_enable_gl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/leo_lin_enable_gl.h
  )
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_GL_ENABLE_LINUX_SRCS})
endif()
