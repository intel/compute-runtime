#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_command_encoder.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_atomic.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_command_buffer.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_enable_raytracing.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_media_interface_descriptor.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_mi_flush_dw.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_semaphore.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_set_mmio.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_states.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_implicit_scaling.cpp
)

add_subdirectories()
