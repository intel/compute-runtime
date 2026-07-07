#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_SHARINGS_GL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_gl_api_intel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_gl_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_arb_sync_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_cl_image_format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_context_guard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_sync_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gl_texture.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_GL_SRCS})

add_subdirectories()
set_property(GLOBAL PROPERTY LEO_SHARINGS_GL_SRCS ${LEO_SHARINGS_GL_SRCS})
