/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include <atomic>

namespace NEO {
extern off_t lseekReturn;
extern std::atomic<int> lseekCalledCount;
extern std::atomic<int> closeInputFd;
extern std::atomic<int> closeCalledCount;
extern std::vector<void *> mmapVector;

inline void *mmapMock(void *addr, size_t length, int prot, int flags, int fd, off_t offset) noexcept {
    if (addr) {
        return addr;
    }
    void *ptr = nullptr;
    if (length > 0) {
        ptr = alignedMalloc(length, MemoryConstants::pageSize64k);
        mmapVector.push_back(ptr);
    }
    return ptr;
}

inline int munmapMock(void *addr, size_t length) noexcept {
    if (length > 0) {
        auto ptrIt = std::find(mmapVector.begin(), mmapVector.end(), addr);
        if (ptrIt != mmapVector.end()) {
            mmapVector.erase(ptrIt);
            alignedFree(addr);
        }
    }
    return 0;
}

inline off_t lseekMock(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    if ((fd == closeInputFd) && (closeCalledCount > 0)) {
        return 0;
    }
    return lseekReturn;
}
inline int closeMock(int fd) {
    closeInputFd = fd;
    closeCalledCount++;
    return 0;
}

class ExecutionEnvironment;
class BufferObject;
class DrmGemCloseWorker;

class TestedDrmMemoryManager : public MemoryManagerCreate<DrmMemoryManager> {
  public:
    using DrmMemoryManager::acquireGpuRange;
    using DrmMemoryManager::alignmentSelector;
    using DrmMemoryManager::allocateGraphicsMemory;
    using DrmMemoryManager::allocateGraphicsMemory64kb;
    using DrmMemoryManager::allocateGraphicsMemoryForImage;
    using DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr;
    using DrmMemoryManager::allocateGraphicsMemoryWithAlignment;
    using DrmMemoryManager::allocateGraphicsMemoryWithHostPtr;
    using DrmMemoryManager::allocateMemoryByKMD;
    using DrmMemoryManager::allocUserptr;
    using DrmMemoryManager::createAllocWithAlignment;
    using DrmMemoryManager::createAllocWithAlignmentFromUserptr;
    using DrmMemoryManager::createGraphicsAllocation;
    using DrmMemoryManager::createMultiHostAllocation;
    using DrmMemoryManager::eraseSharedBufferObject;
    using DrmMemoryManager::getDefaultDrmContextId;
    using DrmMemoryManager::getDrm;
    using DrmMemoryManager::getRootDeviceIndex;
    using DrmMemoryManager::getUserptrAlignment;
    using DrmMemoryManager::gfxPartitions;
    using DrmMemoryManager::lockResourceInLocalMemoryImpl;
    using DrmMemoryManager::memoryForPinBBs;
    using DrmMemoryManager::mmapFunction;
    using DrmMemoryManager::munmapFunction;
    using DrmMemoryManager::pinBBs;
    using DrmMemoryManager::pinThreshold;
    using DrmMemoryManager::pushSharedBufferObject;
    using DrmMemoryManager::registerAllocationInOs;
    using DrmMemoryManager::releaseGpuRange;
    using DrmMemoryManager::setDomainCpu;
    using DrmMemoryManager::sharingBufferObjects;
    using DrmMemoryManager::supportsMultiStorageResources;
    using DrmMemoryManager::unlockResourceInLocalMemoryImpl;
    using MemoryManager::allocateGraphicsMemoryInDevicePool;
    using MemoryManager::heapAssigner;
    using MemoryManager::registeredEngines;

    TestedDrmMemoryManager(ExecutionEnvironment &executionEnvironment);
    TestedDrmMemoryManager(bool enableLocalMemory,
                           bool allowForcePin,
                           bool validateHostPtrMemory,
                           ExecutionEnvironment &executionEnvironment);
    void injectPinBB(BufferObject *newPinBB, uint32_t rootDeviceIndex);

    DrmGemCloseWorker *getgemCloseWorker();
    void forceLimitedRangeAllocator(uint64_t range);
    void overrideGfxPartition(GfxPartition *newGfxPartition);

    DrmAllocation *allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType);
    ~TestedDrmMemoryManager() override;
    size_t peekSharedBosSize() {
        size_t size = 0;
        std::unique_lock<std::mutex> lock(mtx);
        size = sharingBufferObjects.size();
        return size;
    }
    void *alignedMallocWrapper(size_t size, size_t alignment) override {
        alignedMallocSizeRequired = size;
        return alignedMallocShouldFail ? nullptr : DrmMemoryManager::alignedMallocWrapper(size, alignment);
    }
    bool alignedMallocShouldFail = false;
    size_t alignedMallocSizeRequired = 0u;
};
} // namespace NEO
