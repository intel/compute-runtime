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

set(RUNTIME_SRCS_UTILITIES_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_intercept.h
  ${CMAKE_CURRENT_SOURCE_DIR}/arrayref.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/directory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/iflist.h
  ${CMAKE_CURRENT_SOURCE_DIR}/idlist.h
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/range.h
  ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/spinlock.h
  ${CMAKE_CURRENT_SOURCE_DIR}/stackvec.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/timer_util.h
  ${CMAKE_CURRENT_SOURCE_DIR}/vec.h
)

set(RUNTIME_SRCS_UTILITIES_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/timer_util.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/cpu_info.cpp
)

set(RUNTIME_SRCS_UTILITIES_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/timer_util.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/cpu_info.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_UTILITIES_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_UTILITIES_BASE ${RUNTIME_SRCS_UTILITIES_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_UTILITIES_LINUX ${RUNTIME_SRCS_UTILITIES_LINUX})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_UTILITIES_WINDOWS})
else()
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_UTILITIES_LINUX})
endif()
