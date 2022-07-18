/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <linux/types.h>
#include <sys/ioctl.h>
namespace NEO {
namespace PrelimI915 {
#include "third_party/uapi/prelim/drm/i915_drm.h"
}
} // namespace NEO
using NEO::PrelimI915::drm_gem_close;
using NEO::PrelimI915::drm_i915_engine_info;
using NEO::PrelimI915::drm_i915_gem_context_create_ext;
using NEO::PrelimI915::drm_i915_gem_context_destroy;
using NEO::PrelimI915::drm_i915_gem_context_param;
using NEO::PrelimI915::drm_i915_gem_create;
using NEO::PrelimI915::drm_i915_gem_engine_class;
using NEO::PrelimI915::drm_i915_gem_execbuffer2;
using NEO::PrelimI915::drm_i915_gem_get_tiling;
using NEO::PrelimI915::drm_i915_gem_memory_class;
using NEO::PrelimI915::drm_i915_gem_mmap_offset;
using NEO::PrelimI915::drm_i915_gem_set_domain;
using NEO::PrelimI915::drm_i915_gem_set_tiling;
using NEO::PrelimI915::drm_i915_gem_userptr;
using NEO::PrelimI915::drm_i915_gem_vm_control;
using NEO::PrelimI915::drm_i915_gem_wait;
using NEO::PrelimI915::drm_i915_getparam_t;
using NEO::PrelimI915::drm_i915_memory_region_info;
using NEO::PrelimI915::drm_i915_query;
using NEO::PrelimI915::drm_i915_query_engine_info;
using NEO::PrelimI915::drm_i915_query_memory_regions;
using NEO::PrelimI915::drm_i915_reg_read;
using NEO::PrelimI915::drm_i915_reset_stats;
using NEO::PrelimI915::drm_prime_handle;
using NEO::PrelimI915::i915_context_engines_load_balance;
using NEO::PrelimI915::i915_context_param_engines;
using NEO::PrelimI915::i915_engine_class_instance;
using NEO::PrelimI915::i915_user_extension;
using NEO::PrelimI915::prelim_drm_i915_debug_engine_info;
using NEO::PrelimI915::prelim_drm_i915_debug_eu_control;
using NEO::PrelimI915::prelim_drm_i915_debug_event;
using NEO::PrelimI915::prelim_drm_i915_debug_event_ack;
using NEO::PrelimI915::prelim_drm_i915_debug_event_client;
using NEO::PrelimI915::prelim_drm_i915_debug_event_context;
using NEO::PrelimI915::prelim_drm_i915_debug_event_context_param;
using NEO::PrelimI915::prelim_drm_i915_debug_event_engines;
using NEO::PrelimI915::prelim_drm_i915_debug_event_eu_attention;
using NEO::PrelimI915::prelim_drm_i915_debug_event_uuid;
using NEO::PrelimI915::prelim_drm_i915_debug_event_vm;
using NEO::PrelimI915::prelim_drm_i915_debug_event_vm_bind;
using NEO::PrelimI915::prelim_drm_i915_debug_read_uuid;
using NEO::PrelimI915::prelim_drm_i915_debug_vm_open;
using NEO::PrelimI915::prelim_drm_i915_debugger_open_param;
using NEO::PrelimI915::prelim_drm_i915_eu_stall_property_id;
using NEO::PrelimI915::prelim_drm_i915_gem_cache_reserve;
using NEO::PrelimI915::prelim_drm_i915_gem_clos_free;
using NEO::PrelimI915::prelim_drm_i915_gem_clos_reserve;
using NEO::PrelimI915::prelim_drm_i915_gem_context_param_acc;
using NEO::PrelimI915::prelim_drm_i915_gem_create_ext;
using NEO::PrelimI915::prelim_drm_i915_gem_create_ext_setparam;
using NEO::PrelimI915::prelim_drm_i915_gem_create_ext_vm_private;
using NEO::PrelimI915::prelim_drm_i915_gem_engine_class;
using NEO::PrelimI915::prelim_drm_i915_gem_execbuffer_ext_user_fence;
using NEO::PrelimI915::prelim_drm_i915_gem_memory_class;
using NEO::PrelimI915::prelim_drm_i915_gem_memory_class_instance;
using NEO::PrelimI915::prelim_drm_i915_gem_object_param;
using NEO::PrelimI915::prelim_drm_i915_gem_vm_advise;
using NEO::PrelimI915::prelim_drm_i915_gem_vm_bind;
using NEO::PrelimI915::prelim_drm_i915_gem_vm_prefetch;
using NEO::PrelimI915::prelim_drm_i915_gem_vm_region_ext;
using NEO::PrelimI915::prelim_drm_i915_gem_wait_user_fence;
using NEO::PrelimI915::prelim_drm_i915_query_distance_info;
using NEO::PrelimI915::prelim_drm_i915_uuid_control;
using NEO::PrelimI915::prelim_drm_i915_vm_bind_ext_set_pat;
using NEO::PrelimI915::prelim_drm_i915_vm_bind_ext_user_fence;
using NEO::PrelimI915::prelim_drm_i915_vm_bind_ext_uuid;
