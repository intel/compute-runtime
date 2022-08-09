#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
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

if(TESTS_XEHP_AND_LATER)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_grf_mode_xe_hp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_media_interface_descriptor_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_mi_flush_dw_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_implicit_scaling_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_fixture_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_fixture_xehp_and_later.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_tests_xehp_and_later_1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_tests_xehp_and_later_2.cpp
  )
endif()

if(TESTS_XE_HP_CORE)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_implicit_scaling_xe_hp.cpp)
endif()

if(TESTS_DG2_AND_LATER)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_dg2_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_dg2_and_later.h
  )
endif()

if(TESTS_DG2)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_dg2.cpp)
endif()

if(TESTS_XE_HPG_CORE)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_implicit_scaling_xe_hpg.cpp)
endif()

if(TESTS_XE_HPC_CORE)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_implicit_scaling_xe_hpc.cpp)
endif()

if(TESTS_PVC_AND_LATER)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_3dstate_btd_pvc_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_pvc_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_tests_pvc_and_later.cpp
  )
endif()

add_subdirectories()