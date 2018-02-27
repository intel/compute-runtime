# Copyright (c) 2017-2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

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
