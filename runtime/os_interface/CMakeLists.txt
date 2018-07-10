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

set(RUNTIME_SRCS_OS_INTERFACE_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/32bit_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/DebugVariables_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/DebugVariables.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.inl  
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_BASE ${RUNTIME_SRCS_OS_INTERFACE_BASE})
add_subdirectories()
