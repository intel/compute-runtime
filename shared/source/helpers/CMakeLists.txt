#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/abort.h
    ${CMAKE_CURRENT_SOURCE_DIR}/address_patch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/addressing_mode_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addressing_mode_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/affinity_mask.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aligned_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/api_gfx_core_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app_resource_defines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app_resource_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app_resource_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/append_operations.h
    ${CMAKE_CURRENT_SOURCE_DIR}/array_count.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aux_translation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/basic_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bcs_ccs_dependency_pair_container.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindless_heaps_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindless_heaps_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bit_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_properties_container.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_flush.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/casts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_aot_config_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_before_xe_hp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_before_xe_hpc.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_disable_subgroup_local_block_io.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_enable_subgroup_local_block_io.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_product_config_default.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_xe_hp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_xe_hpc_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/completion_stamp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_bitfield.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_hierarchy_mode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_model_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enable_product.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_control.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_control.h
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/extendable_enum.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fence_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/file_io_load.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fill_pattern_tag_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/flat_batch_buffer_helper_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/get_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_context_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_context_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hash.h
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_base_address_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_bdw_to_dg2.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_bdw_to_icllp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_pvc_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gpu_page_fault_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpu_page_fault_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_ip_version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_walk_order.h
    ${CMAKE_CURRENT_SOURCE_DIR}/in_order_cmd_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/in_order_cmd_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/l3_range.h
    ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.h
    ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen_special.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen_sse4.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_work_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_work_size.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_properties_helpers_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/mt_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/neo_driver_version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/non_copyable_or_moveable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/options.h
    ${CMAKE_CURRENT_SOURCE_DIR}/path.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pause_on_gpu_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pipe_control_args.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_select_args.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_select_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/populate_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/preamble.h
    ${CMAKE_CURRENT_SOURCE_DIR}/preamble_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/preprocessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ray_tracing_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/register_offsets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/registered_method_dispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sleep.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sleep.h
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address.h
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_bdw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_icllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_skl.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/stdio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surface_format_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_container.h
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_packet_constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/topology_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/uint16_avx2.h
    ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4.h
    ${CMAKE_CURRENT_SOURCE_DIR}/validators.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}hw_cmds.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}device_ids_configs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/engine_group_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/command_encoder_args.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/indirect_detection_versions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}image_helper.cpp
)

if(WIN32)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/path.cpp
  )
else()
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/path.cpp
  )
endif()

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/cache_flush_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/preamble_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_xehp_and_later.inl
  )
endif()

if(SUPPORT_XE_HPG_CORE OR SUPPORT_XE_HPC_CORE)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/preamble_xe_hpg_and_xe_hpc.inl
  )
endif()

if(SUPPORT_DG2_AND_LATER)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_dg2_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_dg2_and_later.inl
  )
endif()

if(SUPPORT_MTL_AND_LATER)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_mtl_and_later.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_pvc_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_pvc_and_later.inl
  )
endif()

if(SUPPORT_XE2_AND_LATER)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_xe2_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_xe2_and_later.inl
  )
endif()

if(SUPPORT_XE2_CORE OR SUPPORT_XE_2HPG_CORE)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/preamble_xe2_hpg.inl
  )
endif()

if(SUPPORT_XE3_AND_LATER)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_xe3_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/preamble_xe3_and_later.inl
  )
endif()

if(SUPPORT_XE3_AND_BEFORE)
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_from_gen12lp_to_xe3.inl
  )
endif()

set(NEO_CORE_HELPERS_GMM_CALLBACKS_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_callbacks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_callbacks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_callbacks.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_callbacks_tgllp_and_later.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_HELPERS_GMM_CALLBACKS_WINDOWS ${NEO_CORE_HELPERS_GMM_CALLBACKS_WINDOWS})
set_property(GLOBAL PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})

add_subdirectories()
