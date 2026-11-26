/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {

class MockDrmAllocation : public DrmAllocation {
  public:
    using DrmAllocation::bufferObjects;
    using DrmAllocation::memoryPool;
    using DrmAllocation::memoryToUnmap;
    using DrmAllocation::registeredBoBindHandles;

    MockDrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, MemoryPool pool);
    MockDrmAllocation(AllocationType allocationType, MemoryPool pool, BufferObjects &bos);
    ~MockDrmAllocation() override;

    void registerBOBindExtHandle(Drm *drm) override;
    void markForCapture() override;
    int bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) override;
    bool prefetchBO(BufferObject *bo, uint32_t vmHandleId, uint32_t subDeviceId) override;

    ADDMETHOD_NOBASE(makeBOsResident, int, 0, (OsContext * osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence));

    bool registerBOBindExtHandleCalled = false;
    bool markedForCapture = false;
    bool bindBOsCalled = false;
    int bindBOsRetValue = 0;
    bool prefetchBOCalled = false;
    std::vector<uint32_t> vmHandleIdsReceived;
    std::vector<uint32_t> subDeviceIdsReceived;
};

} // namespace NEO
