/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
constexpr size_t bigAllocation = 1 * MB;
constexpr uintptr_t dummyAddress = 0xFFFFF000u;
class MemoryAllocation : public GraphicsAllocation {
  public:
    const unsigned long long id;
    size_t sizeToFree = 0;
    const bool uncacheable;

    MemoryAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn,
                     MemoryPool pool, size_t maxOsContextCount)
        : MemoryAllocation(rootDeviceIndex, 1, allocationType, cpuPtrIn, gpuAddress, baseAddress, sizeIn, pool, maxOsContextCount) {}

    MemoryAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, uint64_t canonizedGpuAddress, uint64_t baseAddress, size_t sizeIn,
                     MemoryPool pool, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, canonizedGpuAddress, baseAddress, sizeIn, pool, maxOsContextCount),
          id(0), uncacheable(false) {}

    MemoryAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
        : MemoryAllocation(rootDeviceIndex, 1, allocationType, cpuPtrIn, sizeIn, sharedHandleIn, pool, maxOsContextCount, canonizedGpuAddress) {}

    MemoryAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, sizeIn, sharedHandleIn, pool, maxOsContextCount, canonizedGpuAddress),
          id(0), uncacheable(false) {}

    MemoryAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *driverAllocatedCpuPointer, void *pMem, uint64_t canonizedGpuAddress, size_t memSize,
                     uint64_t count, MemoryPool pool, bool uncacheable, bool flushL3Required, size_t maxOsContextCount)
        : MemoryAllocation(rootDeviceIndex, 1, allocationType, driverAllocatedCpuPointer, pMem, canonizedGpuAddress, memSize,
                           count, pool, uncacheable, flushL3Required, maxOsContextCount) {}

    MemoryAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *driverAllocatedCpuPointer, void *pMem, uint64_t canonizedGpuAddress, size_t memSize,
                     uint64_t count, MemoryPool pool, bool uncacheable, bool flushL3Required, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, pMem, canonizedGpuAddress, 0u, memSize, pool, maxOsContextCount),
          id(count), uncacheable(uncacheable) {

        this->driverAllocatedCpuPointer = driverAllocatedCpuPointer;
        overrideMemoryPool(pool);
        allocationInfo.flags.flushL3Required = flushL3Required;
    }

    void overrideMemoryPool(MemoryPool pool);

    void clearUsageInfo() {
        for (auto &info : usageInfos) {
            info.inspectionId = 0u;
            info.residencyTaskCount = objectNotResident;
            info.taskCount = objectNotUsed;
        }
    }
};

class OsAgnosticMemoryManager : public MemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemory;
    using MemoryManager::heapAssigner;

    OsAgnosticMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, executionEnvironment) {}
    OsAgnosticMemoryManager(bool aubUsage, ExecutionEnvironment &executionEnvironment);
    void initialize(bool aubUsage);
    ~OsAgnosticMemoryManager() override;
    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
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

    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override;
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
