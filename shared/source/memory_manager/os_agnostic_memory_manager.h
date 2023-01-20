/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
class MemoryAllocation;

constexpr size_t bigAllocation = 1 * MB;
constexpr uintptr_t dummyAddress = 0xFFFFF000u;

class OsAgnosticMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::heapAssigner;

    OsAgnosticMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, executionEnvironment) {}
    OsAgnosticMemoryManager(bool aubUsage, ExecutionEnvironment &executionEnvironment);
    void initialize(bool aubUsage);
    ~OsAgnosticMemoryManager() override;
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; }

    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override;
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override;

    void turnOnFakingBigAllocations();

    void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) override;
    void releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex) override;

    AddressRange reserveGpuAddress(const void *requiredStartAddress, size_t size, RootDeviceIndicesContainer rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override;
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override;
    bool is64kbPagesEnabled(const HardwareInfo *hwInfo);

  protected:
    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override;
    GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) override;

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override { return graphicsAllocation.getUnderlyingBuffer(); }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {}
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    MemoryAllocation *createMemoryAllocation(AllocationType allocationType, void *driverAllocatedCpuPointer, void *pMem, uint64_t gpuAddress, size_t memSize,
                                             uint64_t count, MemoryPool pool, uint32_t rootDeviceIndex, bool uncacheable, bool flushL3Required, bool requireSpecificBitness);
    bool fakeBigAllocations = false;

  private:
    unsigned long long counter = 0;
};

} // namespace NEO
