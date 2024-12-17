/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
class MemoryAllocation;

constexpr size_t bigAllocation = 1 * MemoryConstants::megaByte;
constexpr uint64_t dummyAddress = 0xFFFFF000u;

class OsAgnosticMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::heapAssigners;

    OsAgnosticMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, executionEnvironment) {}
    OsAgnosticMemoryManager(bool aubUsage, ExecutionEnvironment &executionEnvironment);
    void initialize(bool aubUsage);
    ~OsAgnosticMemoryManager() override;
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;

    void addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override;
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override;
    void handleFenceCompletion(GraphicsAllocation *allocation) override;

    AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override;

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override;
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override;

    void turnOnFakingBigAllocations();

    void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) override;
    void releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex) override;

    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override;
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override;
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override;
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override;
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override;
    void freeCpuAddress(AddressRange addressRange) override;
    bool is64kbPagesEnabled(const HardwareInfo *hwInfo);

  protected:
    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) override;
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

    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override { return graphicsAllocation.getUnderlyingBuffer(); }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {}
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    MemoryAllocation *createMemoryAllocation(AllocationType allocationType, void *driverAllocatedCpuPointer, void *pMem, uint64_t gpuAddress, size_t memSize,
                                             uint64_t count, MemoryPool pool, uint32_t rootDeviceIndex, bool uncacheable, bool flushL3Required, bool requireSpecificBitness);
    void removeAllocationFromDownloadAllocationsInCsr(GraphicsAllocation *alloc);
    bool fakeBigAllocations = false;

  private:
    unsigned long long counter = 0;
};

} // namespace NEO
