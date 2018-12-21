/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_host_ptr_manager.h"
#include <atomic>

namespace OCLRT {
static off_t lseekReturn = 4096u;
static std::atomic<int> lseekCalledCount(0);
static std::atomic<int> mmapMockCallCount(0);
static std::atomic<int> munmapMockCallCount(0);

off_t lseekMock(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    return lseekReturn;
}
void *mmapMock(void *addr, size_t length, int prot, int flags,
               int fd, long offset) noexcept {
    mmapMockCallCount++;
    return reinterpret_cast<void *>(0x1000);
}

int munmapMock(void *addr, size_t length) noexcept {
    munmapMockCallCount++;
    return 0;
}

int closeMock(int) {
    return 0;
}

class TestedDrmMemoryManager : public DrmMemoryManager {
  public:
    using DrmMemoryManager::allocateGraphicsMemory;
    using DrmMemoryManager::allocateGraphicsMemory64kb;
    using DrmMemoryManager::allocateGraphicsMemoryForImage;
    using DrmMemoryManager::allocateGraphicsMemoryWithHostPtr;
    using DrmMemoryManager::AllocationData;
    using DrmMemoryManager::allocUserptr;
    using DrmMemoryManager::pinThreshold;
    using DrmMemoryManager::setDomainCpu;
    using DrmMemoryManager::sharingBufferObjects;
    using MemoryManager::allocateGraphicsMemoryInDevicePool;

    TestedDrmMemoryManager(Drm *drm, ExecutionEnvironment &executionEnvironment) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
        hostPtrManager.reset(new MockHostPtrManager);
    };
    TestedDrmMemoryManager(Drm *drm, bool allowForcePin, bool validateHostPtrMemory, ExecutionEnvironment &executionEnvironment) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerInactive, allowForcePin, validateHostPtrMemory, executionEnvironment) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
    }

    void unreference(BufferObject *bo) {
        DrmMemoryManager::unreference(bo);
    }

    void injectPinBB(BufferObject *newPinBB) {
        BufferObject *currentPinBB = pinBB;
        pinBB = nullptr;
        DrmMemoryManager::unreference(currentPinBB);
        pinBB = newPinBB;
    }

    DrmGemCloseWorker *getgemCloseWorker() { return this->gemCloseWorker.get(); }
    void forceLimitedRangeAllocator(uint64_t range) { initInternalRangeAllocator(range); }

    Allocator32bit *getDrmInternal32BitAllocator() const { return internal32bitAllocator.get(); }
    AllocatorLimitedRange *getDrmLimitedRangeAllocator() const { return limitedGpuAddressRangeAllocator.get(); }
    DrmAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) {
        bool allocateMemory = ptr == nullptr;
        AllocationData allocationData;
        if (allocationOrigin == AllocationOrigin::EXTERNAL_ALLOCATION) {
            getAllocationData(allocationData, MockAllocationProperties::getPropertiesFor32BitExternalAllocation(size, allocateMemory), 0u, ptr);
        } else {
            getAllocationData(allocationData, MockAllocationProperties::getPropertiesFor32BitInternalAllocation(size, allocateMemory), 0u, ptr);
        }
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }
};
} // namespace OCLRT
