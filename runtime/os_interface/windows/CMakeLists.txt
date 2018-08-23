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

if(KMDAF_HEADERS_DIR)
  set(KMDAF_FILE_SUFFIX "")
else()
  set(KMDAF_FILE_SUFFIX "_stub")
endif()

set(RUNTIME_SRCS_OS_INTERFACE_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d10_11_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d9_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener${KMDAF_FILE_SUFFIX}.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls.h
  ${CMAKE_CURRENT_SOURCE_DIR}/thk_wrapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_32bit_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_engine_mapper.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_defs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_wrapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_interface.cpp
)

if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE
    ${RUNTIME_SRCS_OS_INTERFACE_WINDOWS}
  )
endif()

add_subdirectories()
