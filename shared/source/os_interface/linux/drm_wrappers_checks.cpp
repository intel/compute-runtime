/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915.h"

namespace NEO {

static_assert(sizeof(GemCreate) == sizeof(drm_i915_gem_create));
static_assert(offsetof(GemCreate, size) == offsetof(drm_i915_gem_create, size));
static_assert(offsetof(GemCreate, handle) == offsetof(drm_i915_gem_create, handle));

static_assert(sizeof(GemUserPtr) == sizeof(drm_i915_gem_userptr));
static_assert(offsetof(GemUserPtr, userPtr) == offsetof(drm_i915_gem_userptr, user_ptr));
static_assert(offsetof(GemUserPtr, userSize) == offsetof(drm_i915_gem_userptr, user_size));
static_assert(offsetof(GemUserPtr, flags) == offsetof(drm_i915_gem_userptr, flags));
static_assert(offsetof(GemUserPtr, handle) == offsetof(drm_i915_gem_userptr, handle));

static_assert(sizeof(RegisterRead) == sizeof(drm_i915_reg_read));
static_assert(offsetof(RegisterRead, offset) == offsetof(drm_i915_reg_read, offset));
static_assert(offsetof(RegisterRead, value) == offsetof(drm_i915_reg_read, val));

static_assert(sizeof(GemSetTiling) == sizeof(drm_i915_gem_set_tiling));
static_assert(offsetof(GemSetTiling, handle) == offsetof(drm_i915_gem_set_tiling, handle));
static_assert(offsetof(GemSetTiling, tilingMode) == offsetof(drm_i915_gem_set_tiling, tiling_mode));
static_assert(offsetof(GemSetTiling, stride) == offsetof(drm_i915_gem_set_tiling, stride));
static_assert(offsetof(GemSetTiling, swizzleMode) == offsetof(drm_i915_gem_set_tiling, swizzle_mode));

static_assert(sizeof(GemGetTiling) == sizeof(drm_i915_gem_get_tiling));
static_assert(offsetof(GemGetTiling, handle) == offsetof(drm_i915_gem_get_tiling, handle));
static_assert(offsetof(GemGetTiling, tilingMode) == offsetof(drm_i915_gem_get_tiling, tiling_mode));
static_assert(offsetof(GemGetTiling, swizzleMode) == offsetof(drm_i915_gem_get_tiling, swizzle_mode));
static_assert(offsetof(GemGetTiling, physSwizzleMode) == offsetof(drm_i915_gem_get_tiling, phys_swizzle_mode));

static_assert(sizeof(ExecObject) == sizeof(drm_i915_gem_exec_object2));

static_assert(sizeof(ExecBuffer) == sizeof(drm_i915_gem_execbuffer2));

static_assert(sizeof(QueryItem) == sizeof(drm_i915_query_item));
static_assert(offsetof(QueryItem, queryId) == offsetof(drm_i915_query_item, query_id));
static_assert(offsetof(QueryItem, length) == offsetof(drm_i915_query_item, length));
static_assert(offsetof(QueryItem, flags) == offsetof(drm_i915_query_item, flags));
static_assert(offsetof(QueryItem, dataPtr) == offsetof(drm_i915_query_item, data_ptr));

static_assert(sizeof(EngineClassInstance) == sizeof(i915_engine_class_instance));
static_assert(offsetof(EngineClassInstance, engineClass) == offsetof(i915_engine_class_instance, engine_class));
static_assert(offsetof(EngineClassInstance, engineInstance) == offsetof(i915_engine_class_instance, engine_instance));

static_assert(sizeof(GemContextParamSseu) == sizeof(drm_i915_gem_context_param_sseu));
static_assert(offsetof(GemContextParamSseu, engine) == offsetof(drm_i915_gem_context_param_sseu, engine));
static_assert(offsetof(GemContextParamSseu, flags) == offsetof(drm_i915_gem_context_param_sseu, flags));
static_assert(offsetof(GemContextParamSseu, sliceMask) == offsetof(drm_i915_gem_context_param_sseu, slice_mask));
static_assert(offsetof(GemContextParamSseu, subsliceMask) == offsetof(drm_i915_gem_context_param_sseu, subslice_mask));
static_assert(offsetof(GemContextParamSseu, minEusPerSubslice) == offsetof(drm_i915_gem_context_param_sseu, min_eus_per_subslice));
static_assert(offsetof(GemContextParamSseu, maxEusPerSubslice) == offsetof(drm_i915_gem_context_param_sseu, max_eus_per_subslice));

static_assert(sizeof(QueryTopologyInfo) == sizeof(drm_i915_query_topology_info));
static_assert(offsetof(QueryTopologyInfo, flags) == offsetof(drm_i915_query_topology_info, flags));
static_assert(offsetof(QueryTopologyInfo, maxSlices) == offsetof(drm_i915_query_topology_info, max_slices));
static_assert(offsetof(QueryTopologyInfo, maxSubslices) == offsetof(drm_i915_query_topology_info, max_subslices));
static_assert(offsetof(QueryTopologyInfo, maxEusPerSubslice) == offsetof(drm_i915_query_topology_info, max_eus_per_subslice));
static_assert(offsetof(QueryTopologyInfo, subsliceOffset) == offsetof(drm_i915_query_topology_info, subslice_offset));
static_assert(offsetof(QueryTopologyInfo, subsliceStride) == offsetof(drm_i915_query_topology_info, subslice_stride));
static_assert(offsetof(QueryTopologyInfo, euOffset) == offsetof(drm_i915_query_topology_info, eu_offset));
static_assert(offsetof(QueryTopologyInfo, euStride) == offsetof(drm_i915_query_topology_info, eu_stride));
static_assert(offsetof(QueryTopologyInfo, data) == offsetof(drm_i915_query_topology_info, data));

static_assert(sizeof(MemoryClassInstance) == sizeof(drm_i915_gem_memory_class_instance));
static_assert(offsetof(MemoryClassInstance, memoryClass) == offsetof(drm_i915_gem_memory_class_instance, memory_class));
static_assert(offsetof(MemoryClassInstance, memoryInstance) == offsetof(drm_i915_gem_memory_class_instance, memory_instance));

static_assert(sizeof(GemMmapOffset) == sizeof(drm_i915_gem_mmap_offset));
static_assert(offsetof(GemMmapOffset, handle) == offsetof(drm_i915_gem_mmap_offset, handle));
static_assert(offsetof(GemMmapOffset, pad) == offsetof(drm_i915_gem_mmap_offset, pad));
static_assert(offsetof(GemMmapOffset, offset) == offsetof(drm_i915_gem_mmap_offset, offset));
static_assert(offsetof(GemMmapOffset, flags) == offsetof(drm_i915_gem_mmap_offset, flags));
static_assert(offsetof(GemMmapOffset, extensions) == offsetof(drm_i915_gem_mmap_offset, extensions));

static_assert(sizeof(GemSetDomain) == sizeof(drm_i915_gem_set_domain));
static_assert(offsetof(GemSetDomain, handle) == offsetof(drm_i915_gem_set_domain, handle));
static_assert(offsetof(GemSetDomain, readDomains) == offsetof(drm_i915_gem_set_domain, read_domains));
static_assert(offsetof(GemSetDomain, writeDomain) == offsetof(drm_i915_gem_set_domain, write_domain));

static_assert(sizeof(GemContextParam) == sizeof(drm_i915_gem_context_param));
static_assert(offsetof(GemContextParam, contextId) == offsetof(drm_i915_gem_context_param, ctx_id));
static_assert(offsetof(GemContextParam, size) == offsetof(drm_i915_gem_context_param, size));
static_assert(offsetof(GemContextParam, param) == offsetof(drm_i915_gem_context_param, param));
static_assert(offsetof(GemContextParam, value) == offsetof(drm_i915_gem_context_param, value));

static_assert(sizeof(DrmUserExtension) == sizeof(i915_user_extension));
static_assert(offsetof(DrmUserExtension, nextExtension) == offsetof(i915_user_extension, next_extension));
static_assert(offsetof(DrmUserExtension, name) == offsetof(i915_user_extension, name));
static_assert(offsetof(DrmUserExtension, flags) == offsetof(i915_user_extension, flags));
static_assert(offsetof(DrmUserExtension, reserved) == offsetof(i915_user_extension, rsvd));

static_assert(sizeof(GemContextCreateExtSetParam) == sizeof(drm_i915_gem_context_create_ext_setparam));
static_assert(offsetof(GemContextCreateExtSetParam, base) == offsetof(drm_i915_gem_context_create_ext_setparam, base));
static_assert(offsetof(GemContextCreateExtSetParam, param) == offsetof(drm_i915_gem_context_create_ext_setparam, param));

static_assert(sizeof(GemContextCreateExt) == sizeof(drm_i915_gem_context_create_ext));
static_assert(offsetof(GemContextCreateExt, contextId) == offsetof(drm_i915_gem_context_create_ext, ctx_id));
static_assert(offsetof(GemContextCreateExt, flags) == offsetof(drm_i915_gem_context_create_ext, flags));
static_assert(offsetof(GemContextCreateExt, extensions) == offsetof(drm_i915_gem_context_create_ext, extensions));

static_assert(sizeof(GemContextDestroy) == sizeof(drm_i915_gem_context_destroy));
static_assert(offsetof(GemContextDestroy, contextId) == offsetof(drm_i915_gem_context_destroy, ctx_id));
static_assert(offsetof(GemContextDestroy, reserved) == offsetof(drm_i915_gem_context_destroy, pad));

static_assert(sizeof(GemVmControl) == sizeof(drm_i915_gem_vm_control));
static_assert(offsetof(GemVmControl, extensions) == offsetof(drm_i915_gem_vm_control, extensions));
static_assert(offsetof(GemVmControl, flags) == offsetof(drm_i915_gem_vm_control, flags));
static_assert(offsetof(GemVmControl, vmId) == offsetof(drm_i915_gem_vm_control, vm_id));

static_assert(sizeof(GemWait) == sizeof(drm_i915_gem_wait));
static_assert(offsetof(GemWait, boHandle) == offsetof(drm_i915_gem_wait, bo_handle));
static_assert(offsetof(GemWait, flags) == offsetof(drm_i915_gem_wait, flags));
static_assert(offsetof(GemWait, timeoutNs) == offsetof(drm_i915_gem_wait, timeout_ns));

static_assert(sizeof(ResetStats) == sizeof(drm_i915_reset_stats));
static_assert(offsetof(ResetStats, contextId) == offsetof(drm_i915_reset_stats, ctx_id));
static_assert(offsetof(ResetStats, flags) == offsetof(drm_i915_reset_stats, flags));
static_assert(offsetof(ResetStats, resetCount) == offsetof(drm_i915_reset_stats, reset_count));
static_assert(offsetof(ResetStats, batchActive) == offsetof(drm_i915_reset_stats, batch_active));
static_assert(offsetof(ResetStats, batchPending) == offsetof(drm_i915_reset_stats, batch_pending));
static_assert(offsetof(ResetStats, reserved) == offsetof(drm_i915_reset_stats, pad));

static_assert(sizeof(GetParam) == sizeof(struct drm_i915_getparam));
static_assert(offsetof(GetParam, param) == offsetof(struct drm_i915_getparam, param));
static_assert(offsetof(GetParam, value) == offsetof(struct drm_i915_getparam, value));

static_assert(sizeof(Query) == sizeof(struct drm_i915_query));
static_assert(offsetof(Query, numItems) == offsetof(struct drm_i915_query, num_items));
static_assert(offsetof(Query, flags) == offsetof(struct drm_i915_query, flags));
static_assert(offsetof(Query, itemsPtr) == offsetof(struct drm_i915_query, items_ptr));

static_assert(sizeof(GemClose) == sizeof(drm_gem_close));
static_assert(offsetof(GemClose, handle) == offsetof(drm_gem_close, handle));
static_assert(offsetof(GemClose, reserved) == offsetof(drm_gem_close, pad));

static_assert(sizeof(PrimeHandle) == sizeof(drm_prime_handle));
static_assert(offsetof(PrimeHandle, handle) == offsetof(drm_prime_handle, handle));
static_assert(offsetof(PrimeHandle, flags) == offsetof(drm_prime_handle, flags));
static_assert(offsetof(PrimeHandle, fileDescriptor) == offsetof(drm_prime_handle, fd));

static_assert(sizeof(DrmVersion) == sizeof(drm_version));
static_assert(offsetof(DrmVersion, versionMajor) == offsetof(drm_version, version_major));
static_assert(offsetof(DrmVersion, versionMinor) == offsetof(drm_version, version_minor));
static_assert(offsetof(DrmVersion, versionPatch) == offsetof(drm_version, version_patchlevel));
static_assert(offsetof(DrmVersion, nameLen) == offsetof(drm_version, name_len));
static_assert(offsetof(DrmVersion, name) == offsetof(drm_version, name));
static_assert(offsetof(DrmVersion, dateLen) == offsetof(drm_version, date_len));
static_assert(offsetof(DrmVersion, date) == offsetof(drm_version, date));
static_assert(offsetof(DrmVersion, descLen) == offsetof(drm_version, desc_len));
static_assert(offsetof(DrmVersion, desc) == offsetof(drm_version, desc));

typedef I915_DEFINE_CONTEXT_PARAM_ENGINES(I915ContextParamEngines, 3);
static_assert(sizeof(ContextParamEngines<3>) == sizeof(I915ContextParamEngines));
static_assert(offsetof(ContextParamEngines<3>, extensions) == offsetof(I915ContextParamEngines, extensions));
static_assert(offsetof(ContextParamEngines<3>, engines) == offsetof(I915ContextParamEngines, engines));

typedef I915_DEFINE_CONTEXT_ENGINES_LOAD_BALANCE(I915ContextEnginesLoadBalance, 3);
static_assert(sizeof(ContextEnginesLoadBalance<3>) == sizeof(I915ContextEnginesLoadBalance));
static_assert(offsetof(ContextEnginesLoadBalance<3>, base) == offsetof(I915ContextEnginesLoadBalance, base));
static_assert(offsetof(ContextEnginesLoadBalance<3>, engineIndex) == offsetof(I915ContextEnginesLoadBalance, engine_index));
static_assert(offsetof(ContextEnginesLoadBalance<3>, numSiblings) == offsetof(I915ContextEnginesLoadBalance, num_siblings));
static_assert(offsetof(ContextEnginesLoadBalance<3>, flags) == offsetof(I915ContextEnginesLoadBalance, flags));
static_assert(offsetof(ContextEnginesLoadBalance<3>, engines) == offsetof(I915ContextEnginesLoadBalance, engines));
} // namespace NEO