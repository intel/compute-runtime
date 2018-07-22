/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include <d3dkmthk.h>
#include <map>
#include <mutex>
#include <vector>

namespace OCLRT {
class Gmm;
class Wddm;

class WddmMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::createGraphicsAllocationFromSharedHandle;

    ~WddmMemoryManager();
    WddmMemoryManager(bool enable64kbPages, Wddm *wddm);

    WddmMemoryManager(const WddmMemoryManager &) = delete;
    WddmMemoryManager &operator=(const WddmMemoryManager &) = delete;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) override;
    GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override;
    GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr) override;
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override;
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

    void addAllocationToHostPtrManager(GraphicsAllocation *memory) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) override;
    void *lockResource(GraphicsAllocation *graphicsAllocation) override;
    void unlockResource(GraphicsAllocation *graphicsAllocation) override;

    bool makeResidentResidencyAllocations(ResidencyContainer *allocationsForResidency);
    void makeNonResidentEvictionAllocations();

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    void obtainGpuAddresFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage);

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) override;

    static const D3DGPU_VIRTUAL_ADDRESS minimumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0x0);
    static const D3DGPU_VIRTUAL_ADDRESS maximumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>((sizeof(size_t) == 8) ? 0x7ffffffffff : (D3DGPU_VIRTUAL_ADDRESS)0xffffffff);

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    static void APIENTRY trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification);

    void acquireResidencyLock() {
        bool previousLockValue = false;
        while (!residencyLock.compare_exchange_weak(previousLockValue, true))
            previousLockValue = false;
    }

    void releaseResidencyLock() {
        residencyLock = false;
    }

    bool tryDeferDeletions(D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue, D3DKMT_HANDLE resourceHandle);

    bool isMemoryBudgetExhausted() const override { return memoryBudgetExhausted; }

    bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) override;

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override;

  protected:
    GraphicsAllocation *createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle);
    WddmAllocation *getTrimCandidateHead() {
        uint32_t i = 0;
        size_t size = trimCandidateList.size();

        if (size == 0) {
            return nullptr;
        }
        while ((trimCandidateList[i] == nullptr) && (i < size))
            i++;

        return (WddmAllocation *)trimCandidateList[i];
    }
    void removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList = false);
    void addToTrimCandidateList(GraphicsAllocation *allocation);
    void compactTrimCandidateList();
    void trimResidency(D3DDDI_TRIMRESIDENCYSET_FLAGS flags, uint64_t bytes);
    bool trimResidencyToBudget(uint64_t bytes);
    static bool validateAllocation(WddmAllocation *alloc);
    bool checkTrimCandidateListCompaction();
    void checkTrimCandidateCount();
    bool createWddmAllocation(WddmAllocation *allocation, AllocationOrigin origin);
    ResidencyContainer trimCandidateList;
    std::mutex trimCandidateListMutex;
    std::atomic<bool> residencyLock;
    uint64_t lastPeriodicTrimFenceValue = 0;
    uint32_t trimCandidatesCount = 0;
    bool memoryBudgetExhausted = false;
    AlignedMallocRestrictions mallocRestrictions;

  private:
    Wddm *wddm;
};
} // namespace OCLRT
