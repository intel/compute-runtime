/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"

namespace NEO {

class MockBufferObject : public BufferObject {
  public:
    using BufferObject::bindExtHandles;
    using BufferObject::bindInfo;
    using BufferObject::BufferObject;
    using BufferObject::handle;

    struct ExecParams {
        uint64_t completionGpuAddress = 0;
        uint32_t completionValue = 0;
    };

    std::vector<ExecParams> passedExecParams{};
    MockBufferObject(Drm *drm) : BufferObject(drm, CommonConstants::unsupportedPatIndex, 0, 0, 1) {
    }
    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
             BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, uint32_t completionValue) override {
        passedExecParams.push_back({completionGpuAddress, completionValue});
        return BufferObject::exec(used, startOffset, flags, requiresCoherency, osContext, vmHandleId, drmContextId,
                                  residency, residencyCount, execObjectsStorage, completionGpuAddress, completionValue);
    }
};

class MockDrmAllocation : public DrmAllocation {
  public:
    using DrmAllocation::bufferObjects;
    using DrmAllocation::enabledMemAdviseFlags;
    using DrmAllocation::memoryPool;
    using DrmAllocation::registeredBoBindHandles;

    MockDrmAllocation(AllocationType allocationType, MemoryPool pool) : DrmAllocation(0, allocationType, nullptr, nullptr, 0, static_cast<size_t>(0), pool) {
    }

    void registerBOBindExtHandle(Drm *drm) override {
        registerBOBindExtHandleCalled = true;
        DrmAllocation::registerBOBindExtHandle(drm);
    }

    void markForCapture() override {
        markedForCapture = true;
        DrmAllocation::markForCapture();
    }

    bool registerBOBindExtHandleCalled = false;
    bool markedForCapture = false;
};

} // namespace NEO
