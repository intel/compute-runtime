/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_wrappers.h"

#include "drm/i915_drm.h"

#include <cstddef>

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

bool GemGetTiling::isTilingDisabled() const { return I915_TILING_NONE == tilingMode; }

static_assert(sizeof(ExecObject) == sizeof(drm_i915_gem_exec_object2));

static_assert(sizeof(ExecBuffer) == sizeof(drm_i915_gem_execbuffer2));

static_assert(sizeof(QueryItem) == sizeof(drm_i915_query_item));
static_assert(offsetof(QueryItem, queryId) == offsetof(drm_i915_query_item, query_id));
static_assert(offsetof(QueryItem, length) == offsetof(drm_i915_query_item, length));
static_assert(offsetof(QueryItem, flags) == offsetof(drm_i915_query_item, flags));
static_assert(offsetof(QueryItem, dataPtr) == offsetof(drm_i915_query_item, data_ptr));

} // namespace NEO
