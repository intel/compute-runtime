/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_manager.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

#include <atomic>

namespace NEO {
extern off_t lseekReturn;
extern std::atomic<int> lseekCalledCount;

inline off_t lseekMock(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    return lseekReturn;
}
inline int closeMock(int) {
    return 0;
}

class ExecutionEnvironment;
class BufferObject;
class DrmGemCloseWorker;

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
    using DrmMemoryManager::pinBBs;
    using DrmMemoryManager::pinThreshold;
    using DrmMemoryManager::releaseGpuRange;
    using DrmMemoryManager::setDomainCpu;
    using DrmMemoryManager::sharingBufferObjects;
    using DrmMemoryManager::supportsMultiStorageResources;
    using DrmMemoryManager::unlockResourceInLocalMemoryImpl;
    using MemoryManager::allocateGraphicsMemoryInDevicePool;
    using MemoryManager::registeredEngines;
    using MemoryManager::useInternal32BitAllocator;

    TestedDrmMemoryManager(ExecutionEnvironment &executionEnvironment);
    TestedDrmMemoryManager(bool enableLocalMemory,
                           bool allowForcePin,
                           bool validateHostPtrMemory,
                           ExecutionEnvironment &executionEnvironment);
    void injectPinBB(BufferObject *newPinBB);

    DrmGemCloseWorker *getgemCloseWorker();
    void forceLimitedRangeAllocator(uint64_t range);
    void overrideGfxPartition(GfxPartition *newGfxPartition);

    DrmAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType);
    ~TestedDrmMemoryManager();
};
} // namespace NEO
