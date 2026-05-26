#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(LEO_SHARINGS_GL_WINDOWS_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_gl_api_helper.cpp
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
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_GL_WINDOWS_SRCS})

  set(LEO_SHARINGS_GL_ENABLE_WINDOWS_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/win_enable_gl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/win_enable_gl.h
  )
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_GL_ENABLE_WINDOWS_SRCS})
endif()
set_property(GLOBAL PROPERTY LEO_SHARINGS_GL_WINDOWS_SRCS ${LEO_SHARINGS_GL_WINDOWS_SRCS})
set_property(GLOBAL PROPERTY LEO_SHARINGS_GL_ENABLE_WINDOWS_SRCS ${LEO_SHARINGS_GL_ENABLE_WINDOWS_SRCS})
