/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/basic_math.h"
#include "runtime/memory_manager/memory_manager.h"
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

    MemoryAllocation(bool cpuPtrAllocated, void *pMem, uint64_t gpuAddress, size_t memSize, uint64_t count, MemoryPool::Type pool) : GraphicsAllocation(pMem, gpuAddress, 0u, memSize),
                                                                                                                                     id(count) {
        this->cpuPtrAllocated = cpuPtrAllocated;
        overrideMemoryPool(pool);
    }

    void overrideMemoryPool(MemoryPool::Type pool);
};

typedef std::map<void *, MemoryAllocation *> PointerMap;

class OsAgnosticMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::createGraphicsAllocationFromSharedHandle;

    OsAgnosticMemoryManager(bool enable64kbPages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(enable64kbPages, enableLocalMemory, false, executionEnvironment){};

    OsAgnosticMemoryManager(bool enable64kbPages, bool enableLocalMemory, bool aubUsage, ExecutionEnvironment &executionEnvironment) : MemoryManager(enable64kbPages, enableLocalMemory, executionEnvironment) {
        allocator32Bit = std::unique_ptr<Allocator32bit>(create32BitAllocator(aubUsage));
    };

    ~OsAgnosticMemoryManager() override;
    GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) override;
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override { return nullptr; }
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

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

    Allocator32bit *create32BitAllocator(bool enableLocalMemory);

  private:
    unsigned long long counter = 0;
    bool fakeBigAllocations = false;
};
} // namespace OCLRT
