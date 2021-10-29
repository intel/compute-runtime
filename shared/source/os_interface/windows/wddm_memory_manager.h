/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <map>
#include <mutex>
#include <vector>

namespace NEO {
class Gmm;
class OsContextWin;
class Wddm;

enum class GfxMemoryAllocationMethod : uint32_t {
    UseUmdSystemPtr,
    AllocateByKmd
};

extern const GfxMemoryAllocationMethod preferredAllocationMethod;

class WddmMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemoryWithProperties;

    ~WddmMemoryManager() override;
    WddmMemoryManager(ExecutionEnvironment &executionEnvironment);

    WddmMemoryManager(const WddmMemoryManager &) = delete;
    WddmMemoryManager &operator=(const WddmMemoryManager &) = delete;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, GraphicsAllocation::AllocationType allocType) override;

    void addAllocationToHostPtrManager(GraphicsAllocation *memory) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;

    void obtainGpuAddressFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage);

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override;
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override;

    bool tryDeferDeletions(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle, uint32_t rootDeviceIndex);

    bool isMemoryBudgetExhausted() const override;

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override;

    bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) override;
    void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) override;
    void releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex) override;
    bool isCpuCopyRequired(const void *ptr) override;

    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override { return AddressRange{0, 0}; };
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    bool verifyHandle(osHandle handle, uint32_t rootDeviceIndex, bool ntHandle) override;
    void releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex) override{};
    void createDeviceSpecificMemResources(uint32_t rootDeviceIndex) override{};

  protected:
    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override { return nullptr; }

    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData);
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages);

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    MOCKABLE_VIRTUAL size_t getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const;
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateHugeGraphicsMemory(const AllocationData &allocationData, bool sharedVirtualAddress);

    GraphicsAllocation *createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle, GraphicsAllocation::AllocationType allocationType, uint32_t rootDeviceIndex);
    static bool validateAllocation(WddmAllocation *alloc);
    MOCKABLE_VIRTUAL bool createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr);
    bool mapGpuVirtualAddress(WddmAllocation *graphicsAllocation, const void *requiredGpuPtr);
    bool mapGpuVaForOneHandleAllocation(WddmAllocation *graphicsAllocation, const void *requiredGpuPtr);
    bool mapMultiHandleAllocationWithRetry(WddmAllocation *allocation, const void *requiredGpuPtr);
    bool createGpuAllocationsWithRetry(WddmAllocation *graphicsAllocation);
    AlignedMallocRestrictions mallocRestrictions;

    Wddm &getWddm(uint32_t rootDeviceIndex) const;
};
} // namespace NEO
