/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_memory.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"

#include <d3dkmthk.h>

#include <vector>

namespace NEO {
class Gmm;
class OsContextWin;
class Wddm;

extern const GfxMemoryAllocationMethod preferredAllocationMethod;

class WddmMemoryManager : public MemoryManager, NEO::NonCopyableAndNonMovableClass {
  public:
    using MemoryManager::allocateGraphicsMemoryWithProperties;

    ~WddmMemoryManager() override;
    WddmMemoryManager(ExecutionEnvironment &executionEnvironment);

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;

    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;

    void addAllocationToHostPtrManager(GraphicsAllocation *memory) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;

    void obtainGpuAddressFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage);

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override;
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override;

    bool tryDeferDeletions(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle, uint32_t rootDeviceIndex, AllocationType type);

    bool isMemoryBudgetExhausted() const override;

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override;

    bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) override;
    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override;
    void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) override;
    void releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex) override;
    bool isCpuCopyRequired(const void *ptr) override;
    bool isWCMemory(const void *ptr) override;

    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override;
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override;
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override;
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override;
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override;
    void freeCpuAddress(AddressRange addressRange) override;
    bool verifyHandle(osHandle handle, uint32_t rootDeviceIndex, bool ntHandle) override;
    bool isNTHandle(osHandle handle, uint32_t rootDeviceIndex) override;
    void releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex) override{};
    void createDeviceSpecificMemResources(uint32_t rootDeviceIndex) override{};
    void registerAllocationInOs(GraphicsAllocation *allocation) override;
    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override;
    MOCKABLE_VIRTUAL NTSTATUS createInternalNTHandle(D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle, uint32_t rootDeviceIndex);

  protected:
    GfxMemoryAllocationMethod getPreferredAllocationMethod(const AllocationProperties &allocationProperties) const override;
    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    void unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override;
    void unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override;
    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override;
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override { return nullptr; }

    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData);
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages);

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    MOCKABLE_VIRTUAL size_t getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const;
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateHugeGraphicsMemory(const AllocationData &allocationData, bool sharedVirtualAddress);

    GraphicsAllocation *createAllocationFromHandle(const OsHandleData &osHandleData, bool requireSpecificBitness, bool ntHandle, AllocationType allocationType, uint32_t rootDeviceIndex, void *mapPointer);
    static bool validateAllocation(WddmAllocation *alloc);
    MOCKABLE_VIRTUAL bool createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr);
    MOCKABLE_VIRTUAL bool mapGpuVirtualAddress(WddmAllocation *graphicsAllocation, const void *requiredGpuPtr);
    MOCKABLE_VIRTUAL bool createPhysicalAllocation(WddmAllocation *allocation);
    bool mapGpuVaForOneHandleAllocation(WddmAllocation *graphicsAllocation, const void *requiredGpuPtr);
    bool mapMultiHandleAllocationWithRetry(WddmAllocation *allocation, const void *requiredGpuPtr);
    bool createGpuAllocationsWithRetry(WddmAllocation *graphicsAllocation);
    template <bool is32Bit = is32bit>
    void adjustGpuPtrToHostAddressSpace(WddmAllocation &wddmAllocation, void *&requiredGpuVa);
    bool isStatelessAccessRequired(AllocationType type);
    AlignedMallocRestrictions mallocRestrictions;
    std::unique_ptr<OSMemory> osMemory;

    Wddm &getWddm(uint32_t rootDeviceIndex) const;
};

static_assert(NEO::NonCopyableAndNonMovable<WddmMemoryManager>);

} // namespace NEO
