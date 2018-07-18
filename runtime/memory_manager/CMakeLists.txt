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

set(RUNTIME_SRCS_MEMORY_MANAGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_defines.h
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_constants.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/os_agnostic_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.h
  ${CMAKE_CURRENT_SOURCE_DIR}/residency_container.h
  ${CMAKE_CURRENT_SOURCE_DIR}/surface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/svm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/svm_memory_manager.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_MEMORY_MANAGER})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_MEMORY_MANAGER ${RUNTIME_SRCS_MEMORY_MANAGER})
