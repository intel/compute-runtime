#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(
               ${TARGET_NAME}
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_api_tracing_common.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_core_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_global_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_copy_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_driver_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_event_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_event_api_multi_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_fence_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_barrier_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_image_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_sampler_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_residency_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_module_api_tracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_module_api_multi_tracing.cpp
)
