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
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/basic_math.h"
#include <map>

namespace OCLRT {

constexpr size_t bigAllocation = 1 * MB;
constexpr uintptr_t dummyAddress = 0xFFFFF000u;
class MemoryAllocation : public GraphicsAllocation {
  public:
    unsigned long long id;
    size_t sizeToFree = 0;
    bool uncacheable = false;

    void setSharedHandle(osHandle handle) { this->sharedHandle = handle; }

    MemoryAllocation(bool cpuPtrAllocated, void *pMem, uint64_t gpuAddress, size_t memSize, uint64_t count) : GraphicsAllocation(pMem, gpuAddress, 0u, memSize),
                                                                                                              id(count) {
        this->cpuPtrAllocated = cpuPtrAllocated;
    }
};

typedef std::map<void *, MemoryAllocation *> PointerMap;

class OsAgnosticMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::createGraphicsAllocationFromSharedHandle;

    OsAgnosticMemoryManager(bool enable64kbPages = false) : MemoryManager(enable64kbPages) {
        uint64_t heap32Base = 0x80000000000ul;
        if (sizeof(uintptr_t) == 4) {
            heap32Base = 0x0;
        }
        allocator32Bit = std::unique_ptr<Allocator32bit>(new Allocator32bit(heap32Base, GB - 2 * 4096));
    };
    ~OsAgnosticMemoryManager() override;
    GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin) override;
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override { return nullptr; }
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override;
    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void *lockResource(GraphicsAllocation *graphicsAllocation) override { return nullptr; };
    void unlockResource(GraphicsAllocation *graphicsAllocation) override{};

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) override;

    void turnOnFakingBigAllocations();

  private:
    unsigned long long counter = 0;
    bool fakeBigAllocations = false;
};
} // namespace OCLRT
