/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_host_ptr_manager.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include <atomic>

namespace NEO {
static off_t lseekReturn = 4096u;
static std::atomic<int> lseekCalledCount(0);

inline off_t lseekMock(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    return lseekReturn;
}
inline int closeMock(int) {
    return 0;
}

class TestedDrmMemoryManager : public MemoryManagerCreate<DrmMemoryManager> {
  public:
    using DrmMemoryManager::allocateGraphicsMemory;
    using DrmMemoryManager::allocateGraphicsMemory64kb;
    using DrmMemoryManager::allocateGraphicsMemoryForImage;
    using DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr;
    using DrmMemoryManager::allocateGraphicsMemoryWithAlignment;
    using DrmMemoryManager::allocateGraphicsMemoryWithHostPtr;
    using DrmMemoryManager::allocateShareableMemory;
    using DrmMemoryManager::AllocationData;
    using DrmMemoryManager::allocUserptr;
    using DrmMemoryManager::createGraphicsAllocation;
    using DrmMemoryManager::getDefaultDrmContextId;
    using DrmMemoryManager::getDrm;
    using DrmMemoryManager::gfxPartitions;
    using DrmMemoryManager::lockResourceInLocalMemoryImpl;
    using DrmMemoryManager::pinThreshold;
    using DrmMemoryManager::releaseGpuRange;
    using DrmMemoryManager::setDomainCpu;
    using DrmMemoryManager::sharingBufferObjects;
    using DrmMemoryManager::supportsMultiStorageResources;
    using DrmMemoryManager::unlockResourceInLocalMemoryImpl;
    using MemoryManager::allocateGraphicsMemoryInDevicePool;
    using MemoryManager::registeredEngines;
    using MemoryManager::useInternal32BitAllocator;

    TestedDrmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                                             false,
                                                                                             false,
                                                                                             executionEnvironment) {
        this->lseekFunction = &lseekMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        hostPtrManager.reset(new MockHostPtrManager);
    };

    TestedDrmMemoryManager(bool enableLocalMemory,
                           bool allowForcePin,
                           bool validateHostPtrMemory,
                           ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, enableLocalMemory,
                                                                                             gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                                             allowForcePin,
                                                                                             validateHostPtrMemory,
                                                                                             executionEnvironment) {
        this->lseekFunction = &lseekMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
    }

    void injectPinBB(BufferObject *newPinBB) {
        BufferObject *currentPinBB = pinBB;
        pinBB = nullptr;
        DrmMemoryManager::unreference(currentPinBB, true);
        pinBB = newPinBB;
    }

    DrmGemCloseWorker *getgemCloseWorker() { return this->gemCloseWorker.get(); }
    void forceLimitedRangeAllocator(uint64_t range) { getGfxPartition(0)->init(range, getSizeToReserve(), 0, 1); }
    void overrideGfxPartition(GfxPartition *newGfxPartition) { gfxPartitions[0].reset(newGfxPartition); }

    DrmAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType) {
        bool allocateMemory = ptr == nullptr;
        AllocationData allocationData;
        MockAllocationProperties properties(allocateMemory, size, allocationType);
        getAllocationData(allocationData, properties, ptr, createStorageInfoFromProperties(properties));
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }
};
} // namespace NEO
