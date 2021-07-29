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

if(TESTS_XEHP_PLUS)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_xehp_plus.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_xehp_plus.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_grf_mode_xe_hp_plus.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_xehp_plus.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_media_interface_descriptor_xehp_plus.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_mi_flush_dw_xehp_plus.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_implicit_scaling_xehp_plus.cpp
  )
endif()

add_subdirectories()
