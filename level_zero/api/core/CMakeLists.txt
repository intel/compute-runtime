#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_API
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_barrier_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_cmdlist_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_cmdqueue_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_context_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_copy_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_core_all_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_core_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_device_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_driver_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_event_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_fence_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_image_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_memory_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_module_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_sampler_api_entrypoints.h
)
set_property(GLOBAL PROPERTY L0_SRCS_API ${L0_SRCS_API})
