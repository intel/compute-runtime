/*
 * Copyright (C) 2017-2018 Intel Corporation
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
class Wddm;

using OsContextWin = OsContext::OsContextImpl;

class WddmMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::createGraphicsAllocationFromSharedHandle;

    ~WddmMemoryManager();
    WddmMemoryManager(bool enable64kbPages, bool enableLocalMemory, Wddm *wddm, ExecutionEnvironment &executionEnvironment);

    WddmMemoryManager(const WddmMemoryManager &) = delete;
    WddmMemoryManager &operator=(const WddmMemoryManager &) = delete;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) override;
    GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override;
    GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) override;
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override;
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    void addAllocationToHostPtrManager(GraphicsAllocation *memory) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override;
    void *lockResource(GraphicsAllocation *graphicsAllocation) override;
    void unlockResource(GraphicsAllocation *graphicsAllocation) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    OsContext *getRegisteredOsContext(uint32_t osContextId) { return registeredOsContexts[osContextId]; }

    void obtainGpuAddresFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage);

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) override;

    static const D3DGPU_VIRTUAL_ADDRESS minimumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0x0);
    static const D3DGPU_VIRTUAL_ADDRESS maximumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>((sizeof(size_t) == 8) ? 0x7ffffffffff : (D3DGPU_VIRTUAL_ADDRESS)0xffffffff);

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    static void APIENTRY trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification);

    bool tryDeferDeletions(D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);

    bool isMemoryBudgetExhausted() const override;

    bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) override;

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override;

  protected:
    GraphicsAllocation *createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle);
    static bool validateAllocation(WddmAllocation *alloc);
    bool createWddmAllocation(WddmAllocation *allocation, AllocationOrigin origin);
    AlignedMallocRestrictions mallocRestrictions;

  private:
    Wddm *wddm;
};
} // namespace OCLRT
