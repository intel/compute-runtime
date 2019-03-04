/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_limited_range.h"
#include "runtime/os_interface/linux/drm_neo.h"

#include "drm_gem_close_worker.h"

#include <map>
#include <sys/mman.h>

namespace OCLRT {
class BufferObject;
class Drm;

class DrmMemoryManager : public MemoryManager {
  public:
    DrmMemoryManager(Drm *drm, gemCloseWorkerMode mode, bool forcePinAllowed, bool validateHostPtrMemory, ExecutionEnvironment &executionEnvironment);
    ~DrmMemoryManager() override;

    BufferObject *getPinBB() const;
    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;
    DrmAllocation *allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override;
    GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override { return nullptr; }

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    // drm/i915 ioctl wrappers
    uint32_t unreference(BufferObject *bo, bool synchronousDestroy = false);

    bool isValidateHostMemoryEnabled() const {
        return validateHostPtrMemory;
    }

    DrmGemCloseWorker *peekGemCloseWorker() const { return this->gemCloseWorker.get(); }

  protected:
    BufferObject *findAndReferenceSharedBufferObject(int boHandle);
    BufferObject *createSharedBufferObject(int boHandle, size_t size, bool requireSpecificBitness);
    void eraseSharedBufferObject(BufferObject *bo);
    void pushSharedBufferObject(BufferObject *bo);
    BufferObject *allocUserptr(uintptr_t address, size_t size, uint64_t flags, bool softpin);
    bool setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable);
    uint64_t acquireGpuRange(size_t &size, StorageAllocatorType &allocType, bool requireSpecificBitness);
    void releaseGpuRange(void *address, size_t unmapSize, StorageAllocatorType allocatorType);
    void initInternalRangeAllocator(size_t range);
    void emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const;

    DrmAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    DrmAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    Drm *drm;
    BufferObject *pinBB;
    size_t pinThreshold = 8 * 1024 * 1024;
    bool forcePinEnabled = false;
    const bool validateHostPtrMemory;
    std::unique_ptr<DrmGemCloseWorker> gemCloseWorker;
    decltype(&lseek) lseekFunction = lseek;
    decltype(&mmap) mmapFunction = mmap;
    decltype(&munmap) munmapFunction = munmap;
    decltype(&close) closeFunction = close;
    std::vector<BufferObject *> sharingBufferObjects;
    std::mutex mtx;
    std::unique_ptr<Allocator32bit> internal32bitAllocator;
    std::unique_ptr<AllocatorLimitedRange> limitedGpuAddressRangeAllocator;
};
} // namespace OCLRT
