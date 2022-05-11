/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"

#include "drm/i915_drm.h"

namespace NEO {

uint32_t MockExecObject::getHandle() const {
    auto drmExecObject = reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject->handle;
}
uint64_t MockExecObject::getOffset() const {
    auto drmExecObject = reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject->offset;
}
bool MockExecObject::has48BAddressSupportFlag() const {
    auto drmExecObject = reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject->flags & EXEC_OBJECT_SUPPORTS_48B_ADDRESS;
}
bool MockExecObject::hasCaptureFlag() const {
    auto drmExecObject = reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject->flags & EXEC_OBJECT_CAPTURE;
}
bool MockExecObject::hasAsyncFlag() const {
    auto drmExecObject = reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject->flags & EXEC_OBJECT_ASYNC;
}
} // namespace NEO
