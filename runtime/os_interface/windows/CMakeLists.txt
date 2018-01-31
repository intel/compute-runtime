# Copyright (c) 2018, Intel Corporation
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
cmake_minimum_required (VERSION 3.0)

if(KMDAF_HEADERS_DIR)
  set (RUNTIME_SRCS_KMDAF
    os_interface/windows/kmdaf_listener.cpp
    os_interface/windows/kmdaf_listener.h
  )
else(KMDAF_HEADERS_DIR)
  set (RUNTIME_SRCS_KMDAF
    os_interface/windows/kmdaf_listener_stub.cpp
    os_interface/windows/kmdaf_listener.h
  )
endif(KMDAF_HEADERS_DIR)

if(KMDAF_HEADERS_DIR)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${KMDAF_HEADERS_DIR}
  )
endif(KMDAF_HEADERS_DIR)

source_group("source files\\os_interface" FILES ${RUNTIME_SRCS_KMDAF})

set(RUNTIME_SRCS_KMDAF "${RUNTIME_SRCS_KMDAF}" PARENT_SCOPE)
