/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "drm_gem_close_worker.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include <map>
#include <sys/mman.h>

namespace OCLRT {
class BufferObject;
class Drm;

class DrmMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::createGraphicsAllocationFromSharedHandle;

    DrmMemoryManager(Drm *drm, gemCloseWorkerMode mode, bool forcePinAllowed, bool validateHostPtrMemory, ExecutionEnvironment &executionEnvironment);
    ~DrmMemoryManager() override;

    BufferObject *getPinBB() const;
    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    DrmAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override;
    DrmAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) override;
    DrmAllocation *allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) override { return nullptr; };
    DrmAllocation *allocateGraphicsMemory(size_t size, const void *ptr) override {
        return allocateGraphicsMemory(size, ptr, false);
    }
    DrmAllocation *allocateGraphicsMemory(size_t size, const void *ptr, bool forcePin) override;
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override;
    DrmAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override;
    GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override { return nullptr; }
    void *lockResource(GraphicsAllocation *graphicsAllocation) override;
    void unlockResource(GraphicsAllocation *graphicsAllocation) override;

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    // drm/i915 ioctl wrappers
    uint32_t unreference(BufferObject *bo, bool synchronousDestroy = false);

    DrmAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) override;
    bool isValidateHostMemoryEnabled() const {
        return validateHostPtrMemory;
    }

    DrmGemCloseWorker *peekGemCloseWorker() { return this->gemCloseWorker.get(); }

  protected:
    BufferObject *findAndReferenceSharedBufferObject(int boHandle);
    BufferObject *createSharedBufferObject(int boHandle, size_t size, bool requireSpecificBitness);
    void eraseSharedBufferObject(BufferObject *bo);
    void pushSharedBufferObject(BufferObject *bo);
    BufferObject *allocUserptr(uintptr_t address, size_t size, uint64_t flags, bool softpin);
    bool setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable);

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
};
} // namespace OCLRT
