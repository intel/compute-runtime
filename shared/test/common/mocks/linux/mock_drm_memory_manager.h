/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <atomic>

namespace NEO {
extern std::atomic<int> closeInputFd;
extern std::atomic<int> closeCalledCount;

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
    using BaseClass = MemoryManagerCreate<DrmMemoryManager>;
    using DrmMemoryManager::acquireGpuRange;
    using DrmMemoryManager::alignmentSelector;
    using DrmMemoryManager::allocateGraphicsMemory;
    using DrmMemoryManager::allocateGraphicsMemory64kb;
    using DrmMemoryManager::allocateGraphicsMemoryForImage;
    using DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr;
    using DrmMemoryManager::allocateGraphicsMemoryWithAlignment;
    using DrmMemoryManager::allocateGraphicsMemoryWithHostPtr;
    using DrmMemoryManager::allocateMemoryByKMD;
    using DrmMemoryManager::allocatePhysicalDeviceMemory;
    using DrmMemoryManager::allocatePhysicalHostMemory;
    using DrmMemoryManager::allocatePhysicalLocalDeviceMemory;
    using DrmMemoryManager::allocationTypeForCompletionFence;
    using DrmMemoryManager::allocUserptr;
    using DrmMemoryManager::checkUnexpectedGpuPageFault;
    using DrmMemoryManager::createAllocWithAlignment;
    using DrmMemoryManager::createAllocWithAlignmentFromUserptr;
    using DrmMemoryManager::createGraphicsAllocation;
    using DrmMemoryManager::createMultiHostAllocation;
    using DrmMemoryManager::createSharedUnifiedMemoryAllocation;
    using DrmMemoryManager::createStorageInfoFromProperties;
    using DrmMemoryManager::eraseSharedBoHandleWrapper;
    using DrmMemoryManager::eraseSharedBufferObject;
    using DrmMemoryManager::getBOTypeFromPatIndex;
    using DrmMemoryManager::getDefaultDrmContextId;
    using DrmMemoryManager::getDrm;
    using DrmMemoryManager::getLocalOnlyRequired;
    using DrmMemoryManager::getRootDeviceIndex;
    using DrmMemoryManager::getUserptrAlignment;
    using DrmMemoryManager::gfxPartitions;
    using DrmMemoryManager::handleFenceCompletion;
    using DrmMemoryManager::localMemBanksCount;
    using DrmMemoryManager::lockBufferObject;
    using DrmMemoryManager::lockResourceImpl;
    using DrmMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory;
    using DrmMemoryManager::mapPhysicalHostMemoryToVirtualMemory;
    using DrmMemoryManager::memoryForPinBBs;
    using DrmMemoryManager::mmapFunction;
    using DrmMemoryManager::munmapFunction;
    using DrmMemoryManager::pinBBs;
    using DrmMemoryManager::pinThreshold;
    using DrmMemoryManager::pushSharedBufferObject;
    using DrmMemoryManager::registerAllocationInOs;
    using DrmMemoryManager::registerSharedBoHandleAllocation;
    using DrmMemoryManager::releaseGpuRange;
    using DrmMemoryManager::retrieveMmapOffsetForBufferObject;
    using DrmMemoryManager::secondaryEngines;
    using DrmMemoryManager::selectAlignmentAndHeap;
    using DrmMemoryManager::setDomainCpu;
    using DrmMemoryManager::sharedBoHandles;
    using DrmMemoryManager::sharingBufferObjects;
    using DrmMemoryManager::supportsMultiStorageResources;
    using DrmMemoryManager::tryToGetBoHandleWrapperWithSharedOwnership;
    using DrmMemoryManager::unlockBufferObject;
    using DrmMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory;
    using DrmMemoryManager::unMapPhysicalHostMemoryFromVirtualMemory;
    using DrmMemoryManager::waitOnCompletionFence;
    using MemoryManager::allocateGraphicsMemoryInDevicePool;
    using MemoryManager::allRegisteredEngines;
    using MemoryManager::heapAssigners;
    using MemoryManager::localMemorySupported;

    StorageInfo createStorageInfoFromPropertiesGeneric(const AllocationProperties &properties) {
        return MemoryManager::createStorageInfoFromProperties(properties);
    }

    TestedDrmMemoryManager(ExecutionEnvironment &executionEnvironment);
    TestedDrmMemoryManager(bool enableLocalMemory,
                           bool allowForcePin,
                           bool validateHostPtrMemory,
                           ExecutionEnvironment &executionEnvironment);
    void injectPinBB(BufferObject *newPinBB, uint32_t rootDeviceIndex);

    DrmGemCloseWorker *getgemCloseWorker();
    void forceLimitedRangeAllocator(uint64_t range);
    void overrideGfxPartition(GfxPartition *newGfxPartition);

    BufferObject *findAndReferenceSharedBufferObject(int boHandle, uint32_t rootDeviceIndex) override;

    DrmAllocation *allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, AllocationType allocationType);
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
    uint32_t unreference(BufferObject *bo, bool synchronousDestroy) override {
        std::unique_lock<std::mutex> lock(unreferenceMtx);
        unreferenceCalled++;
        unreferenceParamsPassed.push_back({bo, synchronousDestroy});
        return DrmMemoryManager::unreference(bo, synchronousDestroy);
    }
    struct UnreferenceParams {
        BufferObject *bo;
        bool synchronousDestroy;
    };
    uint32_t unreferenceCalled = 0u;
    StackVec<UnreferenceParams, 4> unreferenceParamsPassed{};
    void releaseGpuRange(void *ptr, size_t size, uint32_t rootDeviceIndex) override {
        std::unique_lock<std::mutex> lock(releaseGpuRangeMtx);
        releaseGpuRangeCalled++;
        DrmMemoryManager::releaseGpuRange(ptr, size, rootDeviceIndex);
    }
    uint32_t releaseGpuRangeCalled = 0u;
    void alignedFreeWrapper(void *ptr) override {
        std::unique_lock<std::mutex> lock(alignedFreeWrapperMtx);
        alignedFreeWrapperCalled++;
        DrmMemoryManager::alignedFreeWrapper(ptr);
    }
    void closeSharedHandle(GraphicsAllocation *gfxAllocation) override;
    uint32_t alignedFreeWrapperCalled = 0u;
    uint32_t callsToCloseSharedHandle = 0;

    bool failOnfindAndReferenceSharedBufferObject = false;

    bool failOnObtainFdFromHandle = false;

    int obtainFdFromHandle(int boHandle, uint32_t rootDeviceIndex) override {
        if (failOnObtainFdFromHandle) {
            return -1;
        }
        return DrmMemoryManager::obtainFdFromHandle(boHandle, rootDeviceIndex);
    }
    uint64_t acquireGpuRange(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex) override {
        acquireGpuRangeCalledTimes++;
        return DrmMemoryManager::acquireGpuRange(size, rootDeviceIndex, heapIndex);
    }

    uint64_t acquireGpuRangeWithCustomAlignment(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex, size_t alignment) override {
        acquireGpuRangeWithCustomAlignmenCalledTimes++;
        acquireGpuRangeWithCustomAlignmenPassedAlignment = alignment;
        return DrmMemoryManager::acquireGpuRangeWithCustomAlignment(size, rootDeviceIndex, heapIndex, alignment);
    }
    ADDMETHOD(isLimitedRange, bool, true, false, (uint32_t rootDeviceIndex), (rootDeviceIndex));
    ADDMETHOD(isKmdMigrationAvailable, bool, true, false, (uint32_t rootDeviceIndex), (rootDeviceIndex));

    DeviceBitfield computeStorageInfoMemoryBanks(const AllocationProperties &properties, DeviceBitfield preferredBank, DeviceBitfield allBanks) override {
        ++computeStorageInfoMemoryBanksCalled;
        return MemoryManager::computeStorageInfoMemoryBanks(properties, preferredBank, allBanks);
    }

    AllocationStatus registerSysMemAlloc(GraphicsAllocation *allocation) override {
        ++registerSysMemAllocCalled;
        return DrmMemoryManager::registerSysMemAlloc(allocation);
    }

    AllocationStatus registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) override {
        ++registerLocalMemAllocCalled;
        return DrmMemoryManager::registerLocalMemAlloc(allocation, rootDeviceIndex);
    }

    void unregisterAllocation(GraphicsAllocation *allocation) override {
        ++unregisterAllocationCalled;
        DrmMemoryManager::unregisterAllocation(allocation);
    }

    uint32_t acquireGpuRangeCalledTimes = 0u;
    uint32_t acquireGpuRangeWithCustomAlignmenCalledTimes = 0u;
    size_t acquireGpuRangeWithCustomAlignmenPassedAlignment = 0u;
    size_t computeStorageInfoMemoryBanksCalled = 0u;
    size_t registerSysMemAllocCalled = 0u;
    size_t registerLocalMemAllocCalled = 0u;
    size_t unregisterAllocationCalled = 0u;
    ExecutionEnvironment *executionEnvironment = nullptr;

  protected:
    std::mutex unreferenceMtx;
    std::mutex releaseGpuRangeMtx;
    std::mutex alignedFreeWrapperMtx;
    std::mutex callsToCloseSharedHandleMtx;
};

struct MockDrmGemCloseWorker : DrmGemCloseWorker {
    using DrmGemCloseWorker::DrmGemCloseWorker;

    void close(bool blocking) override {
        wasBlocking = blocking;
        DrmGemCloseWorker::close(blocking);
    }
    bool wasBlocking = false;
};

struct MockDrmMemoryManager : DrmMemoryManager {
    using BaseClass = DrmMemoryManager;
    using DrmMemoryManager::DrmMemoryManager;
    using DrmMemoryManager::gemCloseWorker;
    using DrmMemoryManager::mmapFunction;
    using DrmMemoryManager::munmapFunction;
    ADDMETHOD_CONST(emitPinningRequestForBoContainer, SubmissionStatus, true, SubmissionStatus::success, (BufferObject * *bo, uint32_t boCount, uint32_t rootDeviceIndex), (bo, boCount, rootDeviceIndex));
};

} // namespace NEO
