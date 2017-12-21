# Copyright (c) 2017, Intel Corporation
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

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

file(GLOB_RECURSE SHARING_ENABLE_CPPS enable*.cpp)
add_library (${SHARINGS_ENABLE_LIB_NAME} OBJECT ${SHARING_ENABLE_CPPS})
set_target_properties (${SHARINGS_ENABLE_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties (${SHARINGS_ENABLE_LIB_NAME} PROPERTIES FOLDER "opencl runtime")

target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PRIVATE
  ${GMM_SOURCE_DIR}/inc/common
	${KHRONOS_HEADERS_DIR}
	${UMKM_SHAREDDATA_INCLUDE_PATHS}
	${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
)

list (APPEND RUNTIME_SRCS_SHARINGS
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sharing_factory.inl
)

if (WIN32)
  target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PUBLIC ${IGDRCL_SOURCE_DIR}/runtime/os_interface/windows)
else (WIN32)
  target_include_directories(${SHARINGS_ENABLE_LIB_NAME} PUBLIC ${IGDRCL_SOURCE_DIR}/runtime/os_interface/linux)
endif (WIN32)

add_subdirectory(d3d)
add_subdirectory(va)

set(RUNTIME_SRCS_SHARINGS "${RUNTIME_SRCS_SHARINGS}" PARENT_SCOPE)
