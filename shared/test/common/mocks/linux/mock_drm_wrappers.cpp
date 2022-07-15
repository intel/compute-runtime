/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/os_interface/linux/i915.h"

namespace NEO {

uint32_t MockExecObject::getHandle() const {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject.handle;
}
uint64_t MockExecObject::getOffset() const {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject.offset;
}
uint64_t MockExecObject::getReserved() const {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return drmExecObject.rsvd1;
}
bool MockExecObject::has48BAddressSupportFlag() const {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return isValueSet(drmExecObject.flags, EXEC_OBJECT_SUPPORTS_48B_ADDRESS);
}
bool MockExecObject::hasCaptureFlag() const {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return isValueSet(drmExecObject.flags, EXEC_OBJECT_CAPTURE);
}
bool MockExecObject::hasAsyncFlag() const {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(this->data);
    return isValueSet(drmExecObject.flags, EXEC_OBJECT_ASYNC);
}

uint64_t MockExecBuffer::getBuffersPtr() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.buffers_ptr;
}
uint32_t MockExecBuffer::getBufferCount() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.buffer_count;
}
uint32_t MockExecBuffer::getBatchLen() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.batch_len;
}
uint32_t MockExecBuffer::getBatchStartOffset() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.batch_start_offset;
}
uint64_t MockExecBuffer::getFlags() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.flags;
}
uint64_t MockExecBuffer::getCliprectsPtr() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.cliprects_ptr;
}
uint64_t MockExecBuffer::getReserved() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return drmExecBuffer.rsvd1;
}
bool MockExecBuffer::hasUseExtensionsFlag() const {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(this->data);
    return isValueSet(drmExecBuffer.flags, I915_EXEC_USE_EXTENSIONS);
}

} // namespace NEO
