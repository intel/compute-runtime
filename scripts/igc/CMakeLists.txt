# Copyright (c) 2017 - 2018, Intel Corporation
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
#
cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

project(IntelGraphicsCompiler)

add_library(igc SHARED IMPORTED GLOBAL)
set_property(TARGET igc PROPERTY "IMPORTED_LOCATION" "${CMAKE_CURRENT_SOURCE_DIR}/lib/libigdccl.so")
set_property(TARGET igc PROPERTY "INTERFACE_INCLUDE_DIRECTORIES" "${CMAKE_CURRENT_SOURCE_DIR}/inc/")

add_library(opencl_clang SHARED IMPORTED GLOBAL)
set_property(TARGET opencl_clang PROPERTY "IMPORTED_LOCATION" "${CMAKE_CURRENT_SOURCE_DIR}/lib/libopencl_clang.so")

add_library(iga SHARED IMPORTED GLOBAL)
set_property(TARGET iga PROPERTY "IMPORTED_LOCATION" "${CMAKE_CURRENT_SOURCE_DIR}/lib/libiga64.so")

add_library(fcl SHARED IMPORTED GLOBAL)
set_property(TARGET fcl PROPERTY "IMPORTED_LOCATION" "${CMAKE_CURRENT_SOURCE_DIR}/lib/libigdfcl.so")

list(APPEND IGC__IGC_TARGETS "igc")
list(APPEND IGC__IGC_TARGETS "iga")
list(APPEND IGC__IGC_TARGETS "fcl")
list(APPEND IGC__IGC_TARGETS "opencl_clang")

set(IGC__IGC_TARGETS "${IGC__IGC_TARGETS}" PARENT_SCOPE)
