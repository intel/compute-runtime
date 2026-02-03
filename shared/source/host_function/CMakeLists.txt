#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HOST_FUNCTION
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_enablers.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_worker_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_worker_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_worker_counting_semaphore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_worker_counting_semaphore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_scheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_scheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_worker_thread_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_worker_thread_pool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_interface.h
)

set_property(GLOBAL PROPERTY NEO_CORE_HOST_FUNCTION ${NEO_CORE_HOST_FUNCTION})
add_subdirectories()
