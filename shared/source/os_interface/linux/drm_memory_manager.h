/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm_gem_close_worker.h"

#include <limits>
#include <map>
#include <sys/mman.h>

namespace NEO {
class BufferObject;
class Drm;
constexpr uint32_t invalidRootDeviceIndex = std::numeric_limits<uint32_t>::max();

class DrmMemoryManager : public MemoryManager {
  public:
    DrmMemoryManager(gemCloseWorkerMode mode,
                     bool forcePinAllowed,
                     bool validateHostPtrMemory,
                     ExecutionEnvironment &executionEnvironment);
    ~DrmMemoryManager() override;

    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override;
    GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex) override { return nullptr; }

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void commonCleanup() override;

    // drm/i915 ioctl wrappers
    MOCKABLE_VIRTUAL uint32_t unreference(BufferObject *bo, bool synchronousDestroy);

    bool isValidateHostMemoryEnabled() const {
        return validateHostPtrMemory;
    }

    DrmGemCloseWorker *peekGemCloseWorker() const { return this->gemCloseWorker.get(); }
    bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, size_t sizeToCopy) override;

    int obtainFdFromHandle(int boHandle, uint32_t rootDeviceindex);

  protected:
    BufferObject *findAndReferenceSharedBufferObject(int boHandle);
    BufferObject *createSharedBufferObject(int boHandle, size_t size, bool requireSpecificBitness, uint32_t rootDeviceIndex);
    void eraseSharedBufferObject(BufferObject *bo);
    void pushSharedBufferObject(BufferObject *bo);
    BufferObject *allocUserptr(uintptr_t address, size_t size, uint64_t flags, uint32_t rootDeviceIndex);
    bool setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable);
    uint64_t acquireGpuRange(size_t &size, bool requireSpecificBitness, uint32_t rootDeviceIndex, bool requiresStandard64KBHeap);
    MOCKABLE_VIRTUAL void releaseGpuRange(void *address, size_t size, uint32_t rootDeviceIndex);
    void emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const;
    uint32_t getDefaultDrmContextId() const;

    DrmAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateShareableMemory(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void *lockResourceInLocalMemoryImpl(GraphicsAllocation &graphicsAllocation);
    MOCKABLE_VIRTUAL void *lockResourceInLocalMemoryImpl(BufferObject *bo);
    MOCKABLE_VIRTUAL void unlockResourceInLocalMemoryImpl(BufferObject *bo);
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    DrmAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    Drm &getDrm(uint32_t rootDeviceIndex) const;
    uint32_t getRootDeviceIndex(const Drm *drm);

    std::vector<BufferObject *> pinBBs;
    std::vector<void *> memoryForPinBBs;
    size_t pinThreshold = 8 * 1024 * 1024;
    bool forcePinEnabled = false;
    const bool validateHostPtrMemory;
    std::unique_ptr<DrmGemCloseWorker> gemCloseWorker;
    decltype(&lseek) lseekFunction = lseek;
    decltype(&close) closeFunction = close;
    std::vector<BufferObject *> sharingBufferObjects;
    std::mutex mtx;
};
} // namespace NEO
