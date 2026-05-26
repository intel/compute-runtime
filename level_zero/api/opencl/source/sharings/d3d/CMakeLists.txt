#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(LEO_SHARINGS_D3D_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_d3d.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_d3d.h
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_d3d_api.h
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_buffer.h
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing.h
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_surface.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_surface.h
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_texture.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/d3d_texture.h
  )
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_D3D_SRCS})
endif()
set_property(GLOBAL PROPERTY LEO_SHARINGS_D3D_SRCS ${LEO_SHARINGS_D3D_SRCS})
