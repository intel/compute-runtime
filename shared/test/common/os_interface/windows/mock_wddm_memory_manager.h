/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

namespace NEO {
class MockWddmMemoryManager : public MemoryManagerCreate<WddmMemoryManager> {
    using BaseClass = WddmMemoryManager;

  public:
    using BaseClass::alignmentSelector;
    using BaseClass::allocateGraphicsMemoryForNonSvmHostPtr;
    using BaseClass::allocateGraphicsMemoryWithAlignment;
    using BaseClass::allocateGraphicsMemoryWithGpuVa;
    using BaseClass::allocateGraphicsMemoryWithHostPtr;
    using BaseClass::allocateGraphicsMemoryWithProperties;
    using BaseClass::allocateMemoryByKMD;
    using BaseClass::allocatePhysicalDeviceMemory;
    using BaseClass::allocatePhysicalHostMemory;
    using BaseClass::allocatePhysicalLocalDeviceMemory;
    using BaseClass::allRegisteredEngines;
    using BaseClass::createGraphicsAllocation;
    using BaseClass::createPhysicalAllocation;
    using BaseClass::createWddmAllocation;
    using BaseClass::getWddm;
    using BaseClass::gfxPartitions;
    using BaseClass::localMemorySupported;
    using BaseClass::mapPhysicalDeviceMemoryToVirtualMemory;
    using BaseClass::mapPhysicalHostMemoryToVirtualMemory;
    using BaseClass::supportsMultiStorageResources;
    using BaseClass::unMapPhysicalDeviceMemoryFromVirtualMemory;
    using BaseClass::unMapPhysicalHostMemoryFromVirtualMemory;
    using MemoryManagerCreate<WddmMemoryManager>::MemoryManagerCreate;
    using BaseClass::executionEnvironment;
    using BaseClass::getHugeGfxMemoryChunkSize;
    using BaseClass::getPreferredAllocationMethod;
    using BaseClass::isStatelessAccessRequired;
    using BaseClass::selectAlignmentAndHeap;

    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override {
        allocationGraphicsMemory64kbCreated = true;
        return BaseClass::allocateGraphicsMemory64kb(allocationData);
    }
    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override {
        if (allocateGraphicsMemoryInNonDevicePool) {
            status = AllocationStatus::RetryInNonDevicePool;
            return nullptr;
        }
        return BaseClass::allocateGraphicsMemoryInDevicePool(allocationData, status);
    }

    size_t hugeGfxMemoryChunkSize = BaseClass::getHugeGfxMemoryChunkSize(preferredAllocationMethod);
    size_t getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const override { return hugeGfxMemoryChunkSize; }

    MockWddmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, false, executionEnvironment) {
        hostPtrManager.reset(new MockHostPtrManager);
    };
    void setDeferredDeleter(DeferredDeleter *deleter) {
        this->deferredDeleter.reset(deleter);
    }
    void setForce32bitAllocations(bool newValue) {
        this->force32bitAllocations = newValue;
    }
    bool validateAllocationMock(WddmAllocation *graphicsAllocation) {
        return this->validateAllocation(graphicsAllocation);
    }
    GraphicsAllocation *allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, AllocationType allocationType) {
        bool allocateMemory = ptr == nullptr;
        AllocationData allocationData;
        MockAllocationProperties properties(rootDeviceIndex, allocateMemory, size, allocationType);
        getAllocationData(allocationData, properties, ptr, createStorageInfoFromProperties(properties));
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        freeGraphicsMemoryImplCalled++;
        BaseClass::freeGraphicsMemoryImpl(gfxAllocation);
    }

    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override {
        copyMemoryToAllocationBanksCalled++;
        return BaseClass::copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask);
    }

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override {
        BaseClass::freeGraphicsMemoryImpl(gfxAllocation, isImportedAllocation);
    }

    GraphicsAllocation *allocateHugeGraphicsMemory(const AllocationData &allocationData, bool sharedVirtualAddress) override {
        allocateHugeGraphicsMemoryCalled = true;
        return BaseClass::allocateHugeGraphicsMemory(allocationData, sharedVirtualAddress);
    }

    void registerAllocationInOs(GraphicsAllocation *gfxAllocation) override {
        registerAllocationInOsCalled++;
        BaseClass::registerAllocationInOs(gfxAllocation);
    }
    uint32_t copyMemoryToAllocationBanksCalled = 0u;
    uint32_t freeGraphicsMemoryImplCalled = 0u;
    uint32_t registerAllocationInOsCalled = 0;
    bool allocationGraphicsMemory64kbCreated = false;
    bool allocateGraphicsMemoryInNonDevicePool = false;
    bool allocateHugeGraphicsMemoryCalled = false;
};
} // namespace NEO
