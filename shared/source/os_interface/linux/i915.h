/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifndef _UAPI_I915_DRM_H_
#include <linux/types.h>
#include <sys/ioctl.h>
namespace NEO {
namespace I915 {
#include "drm/i915_drm.h"
}
} // namespace NEO
using NEO::I915::drm_gem_close;
using NEO::I915::drm_i915_engine_info;
using NEO::I915::drm_i915_gem_context_create_ext;
using NEO::I915::drm_i915_gem_context_create_ext_setparam;
using NEO::I915::drm_i915_gem_context_destroy;
using NEO::I915::drm_i915_gem_context_param;
using NEO::I915::drm_i915_gem_context_param_sseu;
using NEO::I915::drm_i915_gem_create;
using NEO::I915::drm_i915_gem_create_ext;
using NEO::I915::drm_i915_gem_engine_class;
using NEO::I915::drm_i915_gem_exec_object2;
using NEO::I915::drm_i915_gem_execbuffer2;
using NEO::I915::drm_i915_gem_get_tiling;
using NEO::I915::drm_i915_gem_memory_class;
using NEO::I915::drm_i915_gem_memory_class_instance;
using NEO::I915::drm_i915_gem_mmap_offset;
using NEO::I915::drm_i915_gem_set_domain;
using NEO::I915::drm_i915_gem_set_tiling;
using NEO::I915::drm_i915_gem_userptr;
using NEO::I915::drm_i915_gem_vm_control;
using NEO::I915::drm_i915_gem_wait;
using NEO::I915::drm_i915_getparam;
using NEO::I915::drm_i915_getparam_t;
using NEO::I915::drm_i915_memory_region_info;
using NEO::I915::drm_i915_perf_open_param;
using NEO::I915::drm_i915_query;
using NEO::I915::drm_i915_query_engine_info;
using NEO::I915::drm_i915_query_item;
using NEO::I915::drm_i915_query_memory_regions;
using NEO::I915::drm_i915_query_topology_info;
using NEO::I915::drm_i915_reg_read;
using NEO::I915::drm_i915_reset_stats;
using NEO::I915::drm_prime_handle;
using NEO::I915::drm_version;
using NEO::I915::i915_engine_class_instance;
using NEO::I915::i915_user_extension;

#endif
