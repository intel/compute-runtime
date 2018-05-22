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

set(RUNTIME_SRCS_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/api_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_32bit_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_linux.cpp
)

if(UNIX)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_LINUX ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
add_subdirectories()
