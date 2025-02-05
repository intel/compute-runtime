/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <optional>

namespace NEO {

class MockBufferObjectHandleWrapper : public BufferObjectHandleWrapper {
  public:
    using BufferObjectHandleWrapper::boHandle;
    using BufferObjectHandleWrapper::BufferObjectHandleWrapper;
    using BufferObjectHandleWrapper::controlBlock;

    MockBufferObjectHandleWrapper(BufferObjectHandleWrapper &&rhs)
        : BufferObjectHandleWrapper(std::move(rhs)) {}
};

class MockBufferObject : public BufferObject {
  public:
    using BufferObject::bindExtHandles;
    using BufferObject::bindInfo;
    using BufferObject::BufferObject;
    using BufferObject::handle;

    struct ExecParams {
        uint64_t completionGpuAddress = 0;
        TaskCountType completionValue = 0;
    };

    void setSize(size_t mockSize) {
        size = mockSize;
    }

    std::optional<int> execReturnValue;
    std::vector<ExecParams> passedExecParams{};
    MockBufferObject(uint32_t rootDeviceIndex, Drm *drm) : BufferObject(rootDeviceIndex, drm, CommonConstants::unsupportedPatIndex, 0, 0, 1) {
    }
    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
             BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, TaskCountType completionValue) override {
        if (execReturnValue) {
            return *execReturnValue;
        }
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
    using DrmAllocation::memoryToUnmap;
    using DrmAllocation::registeredBoBindHandles;

    MockDrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, MemoryPool pool)
        : DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, nullptr, nullptr, 0, static_cast<size_t>(0), pool) {
    }

    MockDrmAllocation(AllocationType allocationType, MemoryPool pool, BufferObjects &bos)
        : DrmAllocation(0, 0, allocationType, bos, nullptr, 0, static_cast<size_t>(0), pool) {
    }
    ~MockDrmAllocation() override {
        for (uint32_t i = 0; i < getNumGmms(); i++) {
            delete getGmm(i);
        }
        gmms.resize(0);
    }

    void registerBOBindExtHandle(Drm *drm) override {
        registerBOBindExtHandleCalled = true;
        DrmAllocation::registerBOBindExtHandle(drm);
    }

    void markForCapture() override {
        markedForCapture = true;
        DrmAllocation::markForCapture();
    }

    int bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) override {
        bindBOsCalled = true;
        DrmAllocation::bindBOs(osContext, vmHandleId, bufferObjects, bind, forcePagingFence);
        return bindBOsRetValue;
    }

    bool prefetchBO(BufferObject *bo, uint32_t vmHandleId, uint32_t subDeviceId) override {
        prefetchBOCalled = true;
        vmHandleIdsReceived.push_back(vmHandleId);
        subDeviceIdsReceived.push_back(subDeviceId);
        return DrmAllocation::prefetchBO(bo, vmHandleId, subDeviceId);
    }

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
