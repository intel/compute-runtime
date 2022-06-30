#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_EXPERIMENTAL_TRACING
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_barrier_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_barrier_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_cmdlist_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_cmdlist_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_cmdqueue_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_cmdqueue_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_copy_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_copy_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_device_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_device_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_driver_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_driver_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_event_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_event_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_fence_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_fence_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_global_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_global_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_sampler_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_sampler_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_residency_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_residency_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_image_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_image_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_memory_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_memory_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_module_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tracing_module_imp.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_EXPERIMENTAL_TRACING}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_EXPERIMENTAL_TRACING ${L0_SRCS_EXPERIMENTAL_TRACING})
