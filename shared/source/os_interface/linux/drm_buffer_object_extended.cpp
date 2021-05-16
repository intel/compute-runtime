/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_buffer_object.h"

namespace NEO {

void BufferObject::fillExecObjectImpl(drm_i915_gem_exec_object2 &execObject, OsContext *osContext, uint32_t vmHandleId) {}

} // namespace NEO
