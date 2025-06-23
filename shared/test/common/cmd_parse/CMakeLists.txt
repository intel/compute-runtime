#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_libult_common PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_3d_state_btd.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_base.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_compute_mode.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_compute_walker.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_csr_base_address.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_gpgpu_walker.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_l3_control.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_sip.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/gen_cmd_parse.h
               ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.h
               ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse_base.inl
)

if(TESTS_DG2_AND_LATER)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_xe_hpg_and_later.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse_xe_hpg_and_later.inl
  )
endif()

if(TESTS_PVC_AND_LATER)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_mem_fence.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_mem_set.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_state_prefetch.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_system_mem_fence_address.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_xy_block_copy.inl
  )
endif()

if(TESTS_XE2_AND_LATER)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_state_context_data_base_address.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse_xe2_hpg_and_later.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_resource_barrier.inl
  )
endif()
