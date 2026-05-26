#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_API_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/additional_extensions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/additional_extensions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/api_accelerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_command_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_enqueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_mem_obj.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_sampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api_svm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_API_SRCS})
set_property(GLOBAL PROPERTY LEO_API_SRCS ${LEO_API_SRCS})
add_subdirectories()
