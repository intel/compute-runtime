/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <map>
#include <mutex>
#include <vector>

namespace OCLRT {
class Gmm;
class OsContextWin;
class Wddm;

class WddmMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemoryWithProperties;

    ~WddmMemoryManager() override;
    WddmMemoryManager(bool enable64kbPages, bool enableLocalMemory, Wddm *wddm, ExecutionEnvironment &executionEnvironment);

    WddmMemoryManager(const WddmMemoryManager &) = delete;
    WddmMemoryManager &operator=(const WddmMemoryManager &) = delete;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override;

    void addAllocationToHostPtrManager(GraphicsAllocation *memory) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    void obtainGpuAddressFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage);

    static const D3DGPU_VIRTUAL_ADDRESS minimumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0x0);
    static const D3DGPU_VIRTUAL_ADDRESS maximumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>((sizeof(size_t) == 8) ? 0x7ffffffffff : (D3DGPU_VIRTUAL_ADDRESS)0xffffffff);

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    bool tryDeferDeletions(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);

    bool isMemoryBudgetExhausted() const override;

    bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) override;

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override;

    bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, uint32_t sizeToCopy) const override;
    void *reserveCpuAddressRange(size_t size) override;
    void releaseReservedCpuAddressRange(void *reserved, size_t size) override;

  protected:
    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    void freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) override;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    GraphicsAllocation *createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle);
    static bool validateAllocation(WddmAllocation *alloc);
    bool createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr);
    bool mapGpuVirtualAddressWithRetry(WddmAllocation *graphicsAllocation, const void *preferredGpuVirtualAddress);
    uint32_t mapGpuVirtualAddress(WddmAllocation *graphicsAllocation, const void *preferredGpuVirtualAddress, uint32_t startingIndex);
    AlignedMallocRestrictions mallocRestrictions;

  private:
    Wddm *wddm;
};
} // namespace OCLRT
