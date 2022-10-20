/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"

#include <limits>
#include <map>
#include <sys/mman.h>
#include <unistd.h>

namespace NEO {
class BufferObject;
class Drm;
class DrmGemCloseWorker;
class DrmAllocation;

enum class gemCloseWorkerMode;

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
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;
    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
    void closeSharedHandle(GraphicsAllocation *gfxAllocation) override;

    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; }

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
    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override;

    MOCKABLE_VIRTUAL int obtainFdFromHandle(int boHandle, uint32_t rootDeviceindex);
    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override;
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override;
    MOCKABLE_VIRTUAL BufferObject *createBufferObjectInMemoryRegion(Drm *drm, Gmm *gmm, AllocationType allocationType, uint64_t gpuAddress, size_t size,
                                                                    uint32_t memoryBanks, size_t maxOsContextCount, int32_t pairHandle);

    bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) override;

    bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) override;
    bool setMemPrefetch(GraphicsAllocation *gfxAllocation, uint32_t subDeviceId, uint32_t rootDeviceIndex) override;

    [[nodiscard]] std::unique_lock<std::mutex> acquireAllocLock();
    std::vector<GraphicsAllocation *> &getSysMemAllocs();
    std::vector<GraphicsAllocation *> &getLocalMemAllocs(uint32_t rootDeviceIndex);
    void registerSysMemAlloc(GraphicsAllocation *allocation) override;
    void registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) override;
    void unregisterAllocation(GraphicsAllocation *allocation);

    static std::unique_ptr<MemoryManager> create(ExecutionEnvironment &executionEnvironment);

    DrmAllocation *createUSMHostAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool hasMappedPtr);
    void releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex) override;
    void createDeviceSpecificMemResources(uint32_t rootDeviceIndex) override;
    bool allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) override;

  protected:
    MOCKABLE_VIRTUAL BufferObject *findAndReferenceSharedBufferObject(int boHandle, uint32_t rootDeviceIndex);
    void eraseSharedBufferObject(BufferObject *bo);
    void pushSharedBufferObject(BufferObject *bo);
    BufferObject *allocUserptr(uintptr_t address, size_t size, uint32_t rootDeviceIndex);
    bool setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable);
    uint64_t acquireGpuRange(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex);
    MOCKABLE_VIRTUAL void releaseGpuRange(void *address, size_t size, uint32_t rootDeviceIndex);
    void emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const;
    uint32_t getDefaultDrmContextId(uint32_t rootDeviceIndex) const;
    size_t getUserptrAlignment();

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    DrmAllocation *allocateGraphicsMemoryWithAlignmentImpl(const AllocationData &allocationData);
    DrmAllocation *createAllocWithAlignmentFromUserptr(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSVMSize, uint64_t gpuAddress);
    DrmAllocation *createAllocWithAlignment(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSize, uint64_t gpuAddress);
    DrmAllocation *createMultiHostAllocation(const AllocationData &allocationData);
    void obtainGpuAddress(const AllocationData &allocationData, BufferObject *bo, uint64_t gpuAddress);
    GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override;
    GraphicsAllocation *createSharedUnifiedMemoryAllocationLegacy(const AllocationData &allocationData);
    GraphicsAllocation *createSharedUnifiedMemoryAllocation(const AllocationData &allocationData);

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    MOCKABLE_VIRTUAL void *lockBufferObject(BufferObject *bo);
    MOCKABLE_VIRTUAL void unlockBufferObject(BufferObject *bo);
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override;
    void cleanupBeforeReturn(const AllocationData &allocationData, GfxPartition *gfxPartition, DrmAllocation *drmAllocation, GraphicsAllocation *graphicsAllocation, uint64_t &gpuAddress, size_t &sizeAllocated);
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    bool createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount);
    void registerAllocationInOs(GraphicsAllocation *allocation) override;
    void waitOnCompletionFence(GraphicsAllocation *allocation);
    bool allocationTypeForCompletionFence(AllocationType allocationType);
    void makeAllocationResident(GraphicsAllocation *allocation);

    Drm &getDrm(uint32_t rootDeviceIndex) const;
    uint32_t getRootDeviceIndex(const Drm *drm);
    BufferObject *createRootDeviceBufferObject(uint32_t rootDeviceIndex);
    void releaseBufferObject(uint32_t rootDeviceIndex);
    bool retrieveMmapOffsetForBufferObject(uint32_t rootDeviceIndex, BufferObject &bo, uint64_t flags, uint64_t &offset);

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
