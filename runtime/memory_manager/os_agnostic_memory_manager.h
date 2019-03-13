/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
constexpr size_t bigAllocation = 1 * MB;
constexpr uintptr_t dummyAddress = 0xFFFFF000u;
class MemoryAllocation : public GraphicsAllocation {
  public:
    unsigned long long id;
    size_t sizeToFree = 0;
    bool uncacheable = false;

    void setSharedHandle(osHandle handle) { sharingInfo.sharedHandle = handle; }

    MemoryAllocation(AllocationType allocationType, void *driverAllocatedCpuPointer, void *pMem, uint64_t gpuAddress, size_t memSize, uint64_t count, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(allocationType, pMem, gpuAddress, 0u, memSize, pool, multiOsContextCapable),
          id(count) {

        this->driverAllocatedCpuPointer = driverAllocatedCpuPointer;
        overrideMemoryPool(pool);
    }

    void overrideMemoryPool(MemoryPool::Type pool);
};

class OsAgnosticMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;

    OsAgnosticMemoryManager(bool enable64kbPages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(enable64kbPages, enableLocalMemory, false, executionEnvironment) {}

    OsAgnosticMemoryManager(bool enable64kbPages, bool enableLocalMemory, bool aubUsage, ExecutionEnvironment &executionEnvironment) : MemoryManager(enable64kbPages, enableLocalMemory, executionEnvironment) {
        allocator32Bit = std::unique_ptr<Allocator32bit>(create32BitAllocator(aubUsage));
        gfxPartition.init(platformDevices[0]->capabilityTable.gpuAddressSpace);
    }

    ~OsAgnosticMemoryManager() override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override { return nullptr; }

    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override;
    void cleanOsHandles(OsHandleStorage &handleStorage) override;

    uint64_t getSystemSharedMemory() override;
    uint64_t getMaxApplicationAddress() override;
    uint64_t getInternalHeapBaseAddress() override;

    void turnOnFakingBigAllocations();

    Allocator32bit *create32BitAllocator(bool enableLocalMemory);

    void *reserveCpuAddressRange(size_t size) override;
    void releaseReservedCpuAddressRange(void *reserved, size_t size) override;

  protected:
    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override { return ptrOffset(graphicsAllocation.getUnderlyingBuffer(), static_cast<size_t>(graphicsAllocation.getAllocationOffset())); }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {}
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;

  private:
    unsigned long long counter = 0;
    bool fakeBigAllocations = false;
};
} // namespace OCLRT
