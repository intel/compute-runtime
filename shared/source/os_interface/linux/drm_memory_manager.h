/*
 * Copyright (C) 2018-2021 Intel Corporation
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

class DrmMemoryManager : public MemoryManager {
  public:
    DrmMemoryManager(gemCloseWorkerMode mode,
                     bool forcePinAllowed,
                     bool validateHostPtrMemory,
                     ExecutionEnvironment &executionEnvironment);
    ~DrmMemoryManager() override;

    void initialize(gemCloseWorkerMode mode);
    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;
    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
    void closeSharedHandle(GraphicsAllocation *gfxAllocation) override;
    GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, GraphicsAllocation::AllocationType allocType) override { return nullptr; }

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override;
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void commonCleanup() override;

    // drm/i915 ioctl wrappers
    MOCKABLE_VIRTUAL uint32_t unreference(BufferObject *bo, bool synchronousDestroy);

    bool isValidateHostMemoryEnabled() const {
        return validateHostPtrMemory;
    }

    DrmGemCloseWorker *peekGemCloseWorker() const { return this->gemCloseWorker.get(); }
    bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) override;

    MOCKABLE_VIRTUAL int obtainFdFromHandle(int boHandle, uint32_t rootDeviceindex);
    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override;
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override;
    MOCKABLE_VIRTUAL BufferObject *createBufferObjectInMemoryRegion(Drm *drm, uint64_t gpuAddress, size_t size, uint32_t memoryBanks, size_t maxOsContextCount);

    bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) override;

    bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) override;

    std::unique_lock<std::mutex> acquireAllocLock();
    std::vector<GraphicsAllocation *> &getSysMemAllocs();
    std::vector<GraphicsAllocation *> &getLocalMemAllocs(uint32_t rootDeviceIndex);
    void registerSysMemAlloc(GraphicsAllocation *allocation) override;
    void registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) override;
    void unregisterAllocation(GraphicsAllocation *allocation);

    static std::unique_ptr<MemoryManager> create(ExecutionEnvironment &executionEnvironment);

    DrmAllocation *createUSMHostAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool hasMappedPtr);
    void releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex) override;
    void createDeviceSpecificMemResources(uint32_t rootDeviceIndex) override;

  protected:
    BufferObject *findAndReferenceSharedBufferObject(int boHandle);
    void eraseSharedBufferObject(BufferObject *bo);
    void pushSharedBufferObject(BufferObject *bo);
    BufferObject *allocUserptr(uintptr_t address, size_t size, uint64_t flags, uint32_t rootDeviceIndex);
    bool setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable);
    uint64_t acquireGpuRange(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex);
    MOCKABLE_VIRTUAL void releaseGpuRange(void *address, size_t size, uint32_t rootDeviceIndex);
    void emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const;
    uint32_t getDefaultDrmContextId(uint32_t rootDeviceIndex) const;
    size_t getUserptrAlignment();

    DrmAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithAlignmentImpl(const AllocationData &allocationData);
    DrmAllocation *createAllocWithAlignmentFromUserptr(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSVMSize, uint64_t gpuAddress);
    DrmAllocation *createAllocWithAlignment(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSize, uint64_t gpuAddress);
    DrmAllocation *createMultiHostAllocation(const AllocationData &allocationData);
    void obtainGpuAddress(const AllocationData &allocationData, BufferObject *bo, uint64_t gpuAddress);
    DrmAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override;
    GraphicsAllocation *createSharedUnifiedMemoryAllocation(const AllocationData &allocationData);

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void *lockResourceInLocalMemoryImpl(GraphicsAllocation &graphicsAllocation);
    MOCKABLE_VIRTUAL void *lockResourceInLocalMemoryImpl(BufferObject *bo);
    MOCKABLE_VIRTUAL void unlockResourceInLocalMemoryImpl(BufferObject *bo);
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    DrmAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    bool createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount);
    void registerAllocationInOs(GraphicsAllocation *allocation) override;

    Drm &getDrm(uint32_t rootDeviceIndex) const;
    uint32_t getRootDeviceIndex(const Drm *drm);
    BufferObject *createRootDeviceBufferObject(uint32_t rootDeviceIndex);
    void releaseBufferObject(uint32_t rootDeviceIndex);

    std::vector<BufferObject *> pinBBs;
    std::vector<void *> memoryForPinBBs;
    size_t pinThreshold = 8 * 1024 * 1024;
    bool forcePinEnabled = false;
    const bool validateHostPtrMemory;
    std::unique_ptr<DrmGemCloseWorker> gemCloseWorker;
    decltype(&mmap) mmapFunction = mmap;
    decltype(&munmap) munmapFunction = munmap;
    decltype(&lseek) lseekFunction = lseek;
    decltype(&close) closeFunction = close;
    std::vector<BufferObject *> sharingBufferObjects;
    std::mutex mtx;

    std::vector<std::vector<GraphicsAllocation *>> localMemAllocs;
    std::vector<GraphicsAllocation *> sysMemAllocs;
    std::mutex allocMutex;
};
} // namespace NEO
