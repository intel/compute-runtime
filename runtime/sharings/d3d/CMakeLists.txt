#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(RUNTIME_SRCS_SHARINGS_D3D
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_d3d_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_surface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_surface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_texture.h
  )
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_D3D})
endif()
