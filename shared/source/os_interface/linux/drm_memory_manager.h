/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/os_memory.h"

#include <map>
#include <sys/mman.h>
#include <unistd.h>

namespace NEO {
class BufferObject;
class Drm;
class DrmGemCloseWorker;
class DrmAllocation;
class OsContextLinux;
enum class AtomicAccessMode : uint32_t;

enum class GemCloseWorkerMode;

struct BoHandleDeviceIndexPairComparer {
    bool operator()(std::pair<int, uint32_t> const &lhs, std::pair<int, uint32_t> const &rhs) const {
        return (lhs.first < rhs.first) || (lhs.second < rhs.second);
    }
};

class DrmMemoryManager : public MemoryManager {
  public:
    DrmMemoryManager(GemCloseWorkerMode mode,
                     bool forcePinAllowed,
                     bool validateHostPtrMemory,
                     ExecutionEnvironment &executionEnvironment);
    ~DrmMemoryManager() override;

    void initialize(GemCloseWorkerMode mode);
    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;
    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;
    void closeSharedHandle(GraphicsAllocation *gfxAllocation) override;
    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override;

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override;
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void commonCleanup() override;

    // drm/i915 ioctl wrappers
    MOCKABLE_VIRTUAL uint32_t unreference(BufferObject *bo, bool synchronousDestroy);

    void registerIpcExportedAllocation(GraphicsAllocation *graphicsAllocation) override;

    bool isValidateHostMemoryEnabled() const {
        return validateHostPtrMemory;
    }

    static bool isGemCloseWorkerSupported();
    DrmGemCloseWorker *peekGemCloseWorker() const { return this->gemCloseWorker.get(); }
    bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) override;
    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override;

    MOCKABLE_VIRTUAL int obtainFdFromHandle(int boHandle, uint32_t rootDeviceindex);
    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override;
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override;
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override;
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override;
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override;
    void freeCpuAddress(AddressRange addressRange) override;
    MOCKABLE_VIRTUAL BufferObject *createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex, Gmm *gmm, AllocationType allocationType, uint64_t gpuAddress, size_t size,
                                                                    DeviceBitfield memoryBanks, size_t maxOsContextCount, int32_t pairHandle, bool isSystemMemoryPool, bool isUsmHostAllocation);

    bool hasPageFaultsEnabled(const Device &neoDevice) override;
    bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) override;

    bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) override;
    bool setSharedSystemMemAdvise(const void *ptr, const size_t size, MemAdvise memAdviseOp, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override;
    bool setMemPrefetch(GraphicsAllocation *gfxAllocation, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override;
    bool prefetchSharedSystemAlloc(const void *ptr, const size_t size, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override;
    bool setAtomicAccess(GraphicsAllocation *gfxAllocation, size_t size, AtomicAccessMode mode, uint32_t rootDeviceIndex) override;
    bool setSharedSystemAtomicAccess(const void *ptr, const size_t size, AtomicAccessMode mode, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override;
    [[nodiscard]] std::unique_lock<std::mutex> acquireAllocLock();
    std::vector<GraphicsAllocation *> &getSysMemAllocs();
    std::vector<GraphicsAllocation *> &getLocalMemAllocs(uint32_t rootDeviceIndex);
    AllocationStatus registerSysMemAlloc(GraphicsAllocation *allocation) override;
    AllocationStatus registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) override;
    MOCKABLE_VIRTUAL void unregisterAllocation(GraphicsAllocation *allocation);

    static std::unique_ptr<MemoryManager> create(ExecutionEnvironment &executionEnvironment);

    DrmAllocation *createUSMHostAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, void *mappedPtr, bool reuseSharedAllocation);
    void releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex) override;
    void createDeviceSpecificMemResources(uint32_t rootDeviceIndex) override;
    void releaseDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) override;
    bool reInitDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) override;
    bool allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) override;
    Drm &getDrm(uint32_t rootDeviceIndex) const;
    size_t getSizeOfChunk(size_t allocSize);
    bool checkAllocationForChunking(size_t allocSize, size_t minSize, bool subDeviceEnabled, bool debugDisabled, bool modeEnabled, bool bufferEnabled);

    MOCKABLE_VIRTUAL void checkUnexpectedGpuPageFault();

    bool allocateInterrupt(uint32_t &outHandle, uint32_t rootDeviceIndex) override;
    bool releaseInterrupt(uint32_t outHandle, uint32_t rootDeviceIndex) override;

    bool createMediaContext(uint32_t rootDeviceIndex, void *controlSharedMemoryBuffer, uint32_t controlSharedMemoryBufferSize, void *controlBatchBuffer, uint32_t controlBatchBufferSize, void *&outDoorbell) override;
    bool releaseMediaContext(uint32_t rootDeviceIndex, void *doorbellHandle) override;

    uint32_t getNumMediaDecoders(uint32_t rootDeviceIndex) const override;
    uint32_t getNumMediaEncoders(uint32_t rootDeviceIndex) const override;

    bool isCompressionSupportedForShareable(bool isShareable) override;
    MOCKABLE_VIRTUAL SubmissionStatus emitPinningRequestForBoContainer(BufferObject **bo, uint32_t boCount, uint32_t rootDeviceIndex) const;

    void getExtraDeviceProperties(uint32_t rootDeviceIndex, uint32_t *moduleId, uint16_t *serverType) override;

    MOCKABLE_VIRTUAL uint64_t acquireGpuRange(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex);
    MOCKABLE_VIRTUAL void releaseGpuRange(void *address, size_t size, uint32_t rootDeviceIndex);

    BufferObject *allocUserptr(uintptr_t address, size_t size, const AllocationType allocationType, uint32_t rootDeviceIndex);
    size_t getUserptrAlignment();

    void drainGemCloseWorker() const override;
    void disableForcePin();

    decltype(&mmap) mmapFunction = mmap;
    decltype(&munmap) munmapFunction = munmap;

  protected:
    void registerSharedBoHandleAllocation(DrmAllocation *drmAllocation);
    BufferObjectHandleWrapper tryToGetBoHandleWrapperWithSharedOwnership(int boHandle, uint32_t rootDeviceIndex);
    void eraseSharedBoHandleWrapper(int boHandle, uint32_t rootDeviceIndex);

    MOCKABLE_VIRTUAL BufferObject *findAndReferenceSharedBufferObject(int boHandle, uint32_t rootDeviceIndex);
    void eraseSharedBufferObject(BufferObject *bo);
    void pushSharedBufferObject(BufferObject *bo);
    bool setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable);
    MOCKABLE_VIRTUAL uint64_t acquireGpuRangeWithCustomAlignment(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex, size_t alignment);
    void emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const;
    uint32_t getDefaultDrmContextId(uint32_t rootDeviceIndex) const;
    OsContextLinux *getDefaultOsContext(uint32_t rootDeviceIndex) const;

    StorageInfo createStorageInfoFromProperties(const AllocationProperties &properties) override;
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
    GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override;
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override;
    void unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override;
    void unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override;
    GraphicsAllocation *createSharedUnifiedMemoryAllocation(const AllocationData &allocationData);

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    MOCKABLE_VIRTUAL void *lockBufferObject(BufferObject *bo);
    MOCKABLE_VIRTUAL void unlockBufferObject(BufferObject *bo);
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    void cleanupBeforeReturn(const AllocationData &allocationData, GfxPartition *gfxPartition, DrmAllocation *drmAllocation, GraphicsAllocation *graphicsAllocation, uint64_t &gpuAddress, size_t &sizeAllocated);
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    bool createDrmChunkedAllocation(Drm *drm, DrmAllocation *allocation, uint64_t boAddress, size_t boSize, size_t maxOsContextCount);
    bool createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount, size_t preferredAlignment);
    void registerAllocationInOs(GraphicsAllocation *allocation) override;
    void waitOnCompletionFence(GraphicsAllocation *allocation);
    bool allocationTypeForCompletionFence(AllocationType allocationType);
    bool makeAllocationResident(GraphicsAllocation *allocation);

    inline std::unique_ptr<Gmm> makeGmmIfSingleHandle(const AllocationData &allocationData, size_t sizeAligned);
    inline std::unique_ptr<DrmAllocation> makeDrmAllocation(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm, uint64_t gpuAddress, size_t sizeAligned);
    uint32_t getRootDeviceIndex(const Drm *drm);
    BufferObject *createRootDeviceBufferObject(uint32_t rootDeviceIndex);
    void releaseBufferObject(uint32_t rootDeviceIndex);
    bool retrieveMmapOffsetForBufferObject(uint32_t rootDeviceIndex, BufferObject &bo, uint64_t flags, uint64_t &offset);
    BufferObject::BOType getBOTypeFromPatIndex(uint64_t patIndex, bool isPatIndexSupported) const;
    void setLocalMemBanksCount(uint32_t rootDeviceIndex);
    bool getLocalOnlyRequired(AllocationType allocationType, const ProductHelper &productHelper, const ReleaseHelper *releaseHelper, bool preferCompressed) const override;

    std::vector<BufferObject *> pinBBs;
    std::vector<void *> memoryForPinBBs;
    size_t pinThreshold = 8 * 1024 * 1024;
    bool forcePinEnabled = false;
    const bool validateHostPtrMemory;
    std::unique_ptr<DrmGemCloseWorker> gemCloseWorker;
    std::unique_ptr<OSMemory> osMemory;
    decltype(&close) closeFunction = close;
    std::vector<BufferObject *> sharingBufferObjects;
    std::mutex mtx;

    std::map<std::pair<int, uint32_t>, BufferObjectHandleWrapper, BoHandleDeviceIndexPairComparer> sharedBoHandles;
    std::vector<std::vector<GraphicsAllocation *>> localMemAllocs;
    std::vector<size_t> localMemBanksCount;
    std::vector<GraphicsAllocation *> sysMemAllocs;
    std::mutex allocMutex;
};
} // namespace NEO
