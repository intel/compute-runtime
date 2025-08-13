/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

#include "gtest/gtest.h"

namespace NEO {
class MockDevice;
class MemoryManagerMemHandleMock;
class ExecutionEnvironment;
class SVMAllocsManager;
} // namespace NEO

namespace L0 {
struct Device;
struct DriverHandle;

namespace ult {

struct DriverHandleGetFdMock : public L0::DriverHandleImp {
    void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle,
                         NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) override;

    const int mockFd = 57;
    std::pair<void *, int> allocationMap;
    NEO::AllocationType allocationTypeRequested = NEO::AllocationType::unknown;
};

struct ContextFdMock : public L0::ContextImp {
    ContextFdMock(DriverHandleGetFdMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override;

    ze_result_t allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                             size_t size,
                             size_t alignment, void **ptr) override;

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override;

    ze_result_t getImageAllocProperties(Image *image,
                                        ze_image_allocation_ext_properties_t *pAllocProperties) override;

    ze_result_t closeIpcMemHandle(const void *ptr) override {
        return ZE_RESULT_SUCCESS;
    }
    bool memPropTest = false;
    DriverHandleGetFdMock *driverHandle = nullptr;
};

struct MemoryExportImportTest : public ::testing::Test {
    void SetUp() override;

    void TearDown() override {
    }
    std::unique_ptr<DriverHandleGetFdMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextFdMock> context;
};

struct DriverHandleGetMemHandleMock : public L0::DriverHandleImp {
    void *importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId) override;
    void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle,
                         NEO::AllocationType allocationType, void *basePointer,
                         NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) override;

    const int mockFd = 57;
    std::pair<void *, int> allocationFdMap;
    uint64_t mockHandle = 57;
    std::pair<void *, uint64_t> allocationHandleMap;
};

struct ContextMemHandleMock : public L0::ContextImp {
    ContextMemHandleMock(DriverHandleGetMemHandleMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override;

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override;

    ze_result_t getImageAllocProperties(Image *image,
                                        ze_image_allocation_ext_properties_t *pAllocProperties) override;

    ze_result_t closeIpcMemHandle(const void *ptr) override {
        return ZE_RESULT_SUCCESS;
    }

    DriverHandleGetMemHandleMock *driverHandle = nullptr;
};

struct MemoryExportImportWSLTest : public ::testing::Test {
    void SetUp() override;

    void TearDown() override;

    std::unique_ptr<DriverHandleGetMemHandleMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextMemHandleMock> context;
    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerMemHandleMock *currMemoryManager = nullptr;
};

struct DriverHandleGetWinHandleMock : public L0::DriverHandleImp {
    void *importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId) override;

    uint64_t mockHandle = 57;
    std::pair<void *, uint64_t> allocationMap;
};

struct ContextHandleMock : public L0::ContextImp {
    ContextHandleMock(DriverHandleGetWinHandleMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override;

    ze_result_t allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                             size_t size,
                             size_t alignment, void **ptr) override;

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override;

    ze_result_t getImageAllocProperties(Image *image,
                                        ze_image_allocation_ext_properties_t *pAllocProperties) override;

    ze_result_t freeMem(const void *ptr) override;

    DriverHandleGetWinHandleMock *driverHandle = nullptr;
};

struct MemoryExportImportWinHandleTest : public ::testing::Test {
    void SetUp() override;

    void TearDown() override {
        driverHandle.reset(nullptr);
    }
    std::unique_ptr<DriverHandleGetWinHandleMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextHandleMock> context;
};

struct DriverHandleGetIpcHandleMock : public DriverHandleImp {
    void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle,
                         NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData) override;

    const int mockFd = 999;
    std::pair<void *, int> allocationMap;
};

struct ContextGetIpcHandleMock : public L0::ContextImp {
    ContextGetIpcHandleMock(DriverHandleGetIpcHandleMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override;

    ze_result_t getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) override;

    DriverHandleGetIpcHandleMock *driverHandle = nullptr;
};

class MemoryManagerIpcMock : public NEO::MemoryManager {
  public:
    MemoryManagerIpcMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}
    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override {
        return {};
    }
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
        return {};
    }
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override {
        *heap = HeapIndex::heapStandard;
        return MemoryConstants::pageSize64k;
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override { return {}; }
    void freeCpuAddress(AddressRange addressRange) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    bool unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override { return false; };
    bool unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};
};

class IpcImplicitScalingMockGraphicsAllocation : public NEO::MemoryAllocation {
  public:
    using MemoryAllocation::allocationOffset;
    using MemoryAllocation::allocationType;
    using MemoryAllocation::aubInfo;
    using MemoryAllocation::gpuAddress;
    using MemoryAllocation::MemoryAllocation;
    using MemoryAllocation::memoryPool;
    using MemoryAllocation::objectNotResident;
    using MemoryAllocation::objectNotUsed;
    using MemoryAllocation::size;
    using MemoryAllocation::usageInfos;

    IpcImplicitScalingMockGraphicsAllocation()
        : NEO::MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu) {}

    IpcImplicitScalingMockGraphicsAllocation(void *buffer, size_t sizeIn)
        : NEO::MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

    IpcImplicitScalingMockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn)
        : NEO::MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, gpuAddr, 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

    IpcImplicitScalingMockGraphicsAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : NEO::MemoryAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

    uint32_t getNumHandles() override {
        return 2u;
    }

    bool isResident(uint32_t contextId) const override {
        return false;
    }
};

class MemoryManagerOpenIpcMock : public MemoryManagerIpcMock {
  public:
    MemoryManagerOpenIpcMock(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerIpcMock(executionEnvironment) {}

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override;

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *externalPtr) override;

    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData,
                                                                      const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation,
                                                                      bool reuseSharedAllocation, void *mapPointer) override;
    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                               AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation) override {
        delete gfxAllocation;
    }

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override {
        delete gfxAllocation;
    }

    uint64_t sharedHandleAddress = 0x1234;

    bool failOnCreateGraphicsAllocationFromSharedHandle = false;
};

struct ContextIpcMock : public L0::ContextImp {
    ContextIpcMock(DriverHandleImp *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }

    ze_result_t getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) override;

    const int mockFd = 999;
};

struct MemoryOpenIpcHandleTest : public ::testing::Test {
    void SetUp() override;
    void TearDown() override;

    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextIpcMock> context;
};

class MemoryManagerIpcImplicitScalingMock : public NEO::MemoryManager {
  public:
    MemoryManagerIpcImplicitScalingMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}

    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) override {
        return {};
    }
    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
        return {};
    }
    size_t selectAlignmentAndHeap(size_t size, HeapIndex *heap) override {
        *heap = HeapIndex::heapStandard;
        return MemoryConstants::pageSize64k;
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override { return {}; }
    void freeCpuAddress(AddressRange addressRange) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    bool unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) override { return false; };
    bool unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };
    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override { return false; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};

    NEO::GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) override;

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override;

    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                               AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;

    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties,
                                                                      bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;

    void freeGraphicsMemory(NEO::GraphicsAllocation *alloc, bool isImportedAllocation) override {
        delete alloc;
    }

    uint64_t sharedHandleAddress = 0x1234;

    bool failOnCreateGraphicsAllocationFromSharedHandle = false;
};

struct MemoryExportImportImplicitScalingTest : public ::testing::Test {
    void SetUp() override;

    void TearDown() override;

    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;

    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerIpcImplicitScalingMock *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextIpcMock> context;
};

struct MemoryGetIpcHandlePidfdTest : public ::testing::Test {
    void SetUp() override;
    void TearDown() override;

    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextImp> context;
};

} // namespace ult
} // namespace L0
