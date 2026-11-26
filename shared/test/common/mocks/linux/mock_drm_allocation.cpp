/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_allocation.h"

namespace NEO {

MockDrmAllocation::MockDrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, MemoryPool pool)
    : DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, nullptr, nullptr, 0, static_cast<size_t>(0), pool) {
}

MockDrmAllocation::MockDrmAllocation(AllocationType allocationType, MemoryPool pool, BufferObjects &bos)
    : DrmAllocation(0, 0, allocationType, bos, nullptr, 0, static_cast<size_t>(0), pool) {
}

MockDrmAllocation::~MockDrmAllocation() = default;

void MockDrmAllocation::registerBOBindExtHandle(Drm *drm) {
    registerBOBindExtHandleCalled = true;
    DrmAllocation::registerBOBindExtHandle(drm);
}

void MockDrmAllocation::markForCapture() {
    markedForCapture = true;
    DrmAllocation::markForCapture();
}

int MockDrmAllocation::bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) {
    bindBOsCalled = true;
    DrmAllocation::bindBOs(osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
    return bindBOsRetValue;
}

bool MockDrmAllocation::prefetchBO(BufferObject *bo, uint32_t vmHandleId, uint32_t subDeviceId) {
    prefetchBOCalled = true;
    vmHandleIdsReceived.push_back(vmHandleId);
    subDeviceIdsReceived.push_back(subDeviceId);
    return DrmAllocation::prefetchBO(bo, vmHandleId, subDeviceId);
}

} // namespace NEO
