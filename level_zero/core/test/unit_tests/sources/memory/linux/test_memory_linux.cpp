/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

#include <memory>
#include <vector>

namespace L0 {
namespace ult {

class IpcImplicitScalingObtainFdMockGraphicsAllocation : public NEO::DrmAllocation {
  public:
    using NEO::DrmAllocation::bufferObjects;

    IpcImplicitScalingObtainFdMockGraphicsAllocation(uint32_t rootDeviceIndex,
                                                     AllocationType allocationType,
                                                     BufferObject *bo,
                                                     void *ptrIn,
                                                     size_t sizeIn,
                                                     NEO::osHandle sharedHandle,
                                                     MemoryPool pool,
                                                     uint64_t canonizedGpuAddress) : NEO::DrmAllocation(rootDeviceIndex,
                                                                                                        1u /*num gmms*/,
                                                                                                        allocationType,
                                                                                                        bo,
                                                                                                        ptrIn,
                                                                                                        sizeIn,
                                                                                                        sharedHandle,
                                                                                                        pool,
                                                                                                        canonizedGpuAddress) {
        bufferObjects.resize(2u);
    }

    uint32_t getNumHandles() override {
        return 2u;
    }

    bool isResident(uint32_t contextId) const override {
        return false;
    }
};

class MemoryManagerIpcImplicitScalingObtainFdMock : public NEO::DrmMemoryManager {
  public:
    MemoryManagerIpcImplicitScalingObtainFdMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {}

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
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};

    NEO::GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) override {
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcImplicitScalingObtainFdMockGraphicsAllocation(0u,
                                                                          NEO::AllocationType::buffer,
                                                                          nullptr,
                                                                          ptr,
                                                                          size,
                                                                          0u,
                                                                          MemoryPool::system4KBPages,
                                                                          canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        alloc->bufferObjects[0] = mockBos.emplace_back(new MockBufferObject{properties.rootDeviceIndex, &drm}).get();
        alloc->bufferObjects[1] = mockBos.emplace_back(new MockBufferObject{properties.rootDeviceIndex, &drm}).get();
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                               AllocationProperties &properties,
                                                                               bool requireSpecificBitness,
                                                                               bool isHostIpcAllocation,
                                                                               bool reuseSharedAllocation,
                                                                               void *mapPointer) override {
        if (failOnCreateGraphicsAllocationFromSharedHandle) {
            return nullptr;
        }
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcImplicitScalingObtainFdMockGraphicsAllocation(0u,
                                                                          NEO::AllocationType::buffer,
                                                                          nullptr,
                                                                          ptr,
                                                                          size,
                                                                          0u,
                                                                          MemoryPool::system4KBPages,
                                                                          canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        alloc->bufferObjects[0] = mockBos.emplace_back(new MockBufferObject{properties.rootDeviceIndex, &drm}).get();
        alloc->bufferObjects[1] = mockBos.emplace_back(new MockBufferObject{properties.rootDeviceIndex, &drm}).get();
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    void freeGraphicsMemory(NEO::GraphicsAllocation *alloc, bool isImportedAllocation) override {
        delete alloc;
    }

    int obtainFdFromHandle(int boHandle, uint32_t rootDeviceIndex) override {
        if (failOnObtainFdFromHandle) {
            return -1;
        }
        return NEO::DrmMemoryManager::obtainFdFromHandle(boHandle, rootDeviceIndex);
    }
    bool failOnObtainFdFromHandle = false;

    uint64_t sharedHandleAddress = 0x1234;

    bool failOnCreateGraphicsAllocationFromSharedHandle = false;
    std::vector<std::unique_ptr<MockBufferObject>> mockBos;
};

struct MemoryExportImportObtainFdTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;
        debugManager.flags.EnableImplicitScaling.set(1);
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0); // not compatible with MemoryManagerIpcImplicitScalingObtainFdMock

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new NEO::OSInterface());

            auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[i]);
            auto ioctlHelper{drmMock->getIoctlHelper()};
            const auto memoryClassSystem = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem));
            const auto memoryClassDevice = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice));
            std::vector<MemoryRegion> mockMemRegions(3);
            mockMemRegions[0] = {{memoryClassSystem, 0}, 1024};
            mockMemRegions[1] = {{memoryClassDevice, 0}, 2048};
            drmMock->memoryInfo.reset(new MemoryInfo{mockMemRegions, *drmMock});

            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }

        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerIpcImplicitScalingObtainFdMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context = std::make_unique<L0::ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto dev = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(dev->getRootDeviceIndex(), dev->toHandle()));
        }
        device = driverHandle->devices[0];
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;

    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;

    SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerIpcImplicitScalingObtainFdMock *currMemoryManager = nullptr;
    std::unique_ptr<L0::DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<L0::ContextImp> context;
};

TEST_F(MemoryExportImportObtainFdTest,
       givenCallToGetIpcHandlesWithFailureToGetFdHandleThenOutOfHostMemoryIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    currMemoryManager->failOnObtainFdFromHandle = true;

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportObtainFdTest,
       givenCallToGetIpcHandlesWithFdHandleThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    currMemoryManager->failOnObtainFdFromHandle = false;

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportObtainFdTest,
       givenCallToGetIpcHandleWithFailureToGetFdHandleThenOutOfHostMemoryIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    currMemoryManager->failOnObtainFdFromHandle = true;
    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportObtainFdTest,
       givenCallToGetIpcHandleWithFdHandleThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    currMemoryManager->failOnObtainFdFromHandle = false;
    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportObtainFdTest,
       givenCallToEventPoolGetIpcHandleWithFailureToGetFdHandleAtCreateTimeThenUnsupportedIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    currMemoryManager->failOnObtainFdFromHandle = true;

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = EventPool::create(driverHandle.get(),
                                       context.get(),
                                       1,
                                       &deviceHandle,
                                       &eventPoolDesc,
                                       result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);

    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryExportImportObtainFdTest,
       givenCallToEventPoolGetIpcHandleWithFdHandleThenSuccessIsReturned) {
    uint32_t numEvents = 4;
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_IPC,
        numEvents};

    auto deviceHandle = device->toHandle();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = whiteboxCast(EventPool::create(driverHandle.get(),
                                                    context.get(),
                                                    1,
                                                    &deviceHandle,
                                                    &eventPoolDesc,
                                                    result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, eventPool);
    eventPool->isShareableEventMemory = true;

    currMemoryManager->failOnObtainFdFromHandle = false;
    ze_ipc_event_pool_handle_t ipcHandle = {};
    result = eventPool->getIpcHandle(&ipcHandle);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = eventPool->destroy();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryExportImportObtainFdTest,
       whenPeerAllocationForDeviceAllocationIsRequestedThenPeerAllocationIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    currMemoryManager->failOnObtainFdFromHandle = false;
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_NE(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryExportImportObtainFdTest,
       whenPeerAllocationForDeviceAllocationIsRequestedThenNullptrIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    currMemoryManager->failOnObtainFdFromHandle = true;
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_EQ(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

class IpcObtainFdMockGraphicsAllocation : public NEO::DrmAllocation {
  public:
    using NEO::DrmAllocation::bufferObjects;

    IpcObtainFdMockGraphicsAllocation(uint32_t rootDeviceIndex,
                                      AllocationType allocationType,
                                      BufferObject *bo,
                                      void *ptrIn,
                                      size_t sizeIn,
                                      NEO::osHandle sharedHandle,
                                      MemoryPool pool,
                                      uint64_t canonizedGpuAddress) : NEO::DrmAllocation(rootDeviceIndex,
                                                                                         1u /*num gmms*/,
                                                                                         allocationType,
                                                                                         bo,
                                                                                         ptrIn,
                                                                                         sizeIn,
                                                                                         sharedHandle,
                                                                                         pool,
                                                                                         canonizedGpuAddress) {
        bufferObjects.resize(1u);
    }

    uint32_t getNumHandles() override {
        return 1u;
    }

    bool isResident(uint32_t contextId) const override {
        return false;
    }
};

class MemoryManagerIpcObtainFdMock : public NEO::DrmMemoryManager {
  public:
    MemoryManagerIpcObtainFdMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {}

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
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};

    NEO::GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) override {
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcObtainFdMockGraphicsAllocation(0u,
                                                           NEO::AllocationType::buffer,
                                                           nullptr,
                                                           ptr,
                                                           size,
                                                           0u,
                                                           MemoryPool::system4KBPages,
                                                           canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        alloc->bufferObjects[0] = mockBos.emplace_back(new MockBufferObject{properties.rootDeviceIndex, &drm}).get();
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                               AllocationProperties &properties,
                                                                               bool requireSpecificBitness,
                                                                               bool isHostIpcAllocation,
                                                                               bool reuseSharedAllocation,
                                                                               void *mapPointer) override {
        if (failOnCreateGraphicsAllocationFromSharedHandle) {
            return nullptr;
        }
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcObtainFdMockGraphicsAllocation(0u,
                                                           NEO::AllocationType::buffer,
                                                           nullptr,
                                                           ptr,
                                                           size,
                                                           0u,
                                                           MemoryPool::system4KBPages,
                                                           canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        alloc->bufferObjects[0] = mockBos.emplace_back(new MockBufferObject{properties.rootDeviceIndex, &drm}).get();
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    void freeGraphicsMemory(NEO::GraphicsAllocation *alloc, bool isImportedAllocation) override {
        delete alloc;
    }

    int obtainFdFromHandle(int boHandle, uint32_t rootDeviceIndex) override {
        if (failOnObtainFdFromHandle) {
            return -1;
        }
        return NEO::DrmMemoryManager::obtainFdFromHandle(boHandle, rootDeviceIndex);
    }
    bool failOnObtainFdFromHandle = false;

    uint64_t sharedHandleAddress = 0x1234;

    bool failOnCreateGraphicsAllocationFromSharedHandle = false;
    std::vector<std::unique_ptr<MockBufferObject>> mockBos;
};

struct DriverHandleObtaindFdMock : public L0::DriverHandleImp {
    void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) override {
        DeviceBitfield deviceBitfield{0x0};
        AllocationProperties properties(0, MemoryConstants::pageSize,
                                        AllocationType::buffer,
                                        deviceBitfield);
        auto alloc = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        return reinterpret_cast<void *>(alloc->getGpuAddress());
    }
};

struct MemoryObtainFdTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0); // not compatible with MemoryManagerIpcObtainFdMock
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new NEO::OSInterface());

            auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[i]);
            auto ioctlHelper{drmMock->getIoctlHelper()};
            const auto memoryClassSystem = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem));
            const auto memoryClassDevice = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice));
            std::vector<MemoryRegion> mockMemRegions(3);
            mockMemRegions[0] = {{memoryClassSystem, 0}, 1024};
            mockMemRegions[1] = {{memoryClassDevice, 0}, 2048};
            drmMock->memoryInfo.reset(new MemoryInfo{mockMemRegions, *drmMock});

            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }

        driverHandle = std::make_unique<DriverHandleObtaindFdMock>();
        driverHandle->initialize(std::move(devices));

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerIpcObtainFdMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context = std::make_unique<L0::ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto dev = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(dev->getRootDeviceIndex(), dev->toHandle()));
        }
        device = driverHandle->devices[0];
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;

    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;

    SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerIpcObtainFdMock *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleObtaindFdMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<L0::ContextImp> context;
};

TEST_F(MemoryObtainFdTest,
       whenPeerAllocationForDeviceAllocationIsRequestedThenNullptrIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    currMemoryManager->failOnObtainFdFromHandle = true;
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_EQ(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingOpenIpcHandlesWithIpcHandleThenReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandles(device->toHandle(), numIpcHandles, ipcHandles.data(), flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingOpenIpcHandleWithIpcHandleThenAllocationCountIsIncremented) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usmManager = context->getDriverHandle()->getSvmAllocsManager();
    auto currentAllocationCount = usmManager->allocationsCounter.load();

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;

    result = context->openIpcMemHandle(device->toHandle(), ipcHandles[0], flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto newAllocationCount = usmManager->allocationsCounter.load();
    EXPECT_GT(newAllocationCount, currentAllocationCount);
    EXPECT_EQ(usmManager->getSVMAlloc(ipcPtr)->getAllocId(), newAllocationCount);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingOpenIpcHandleWithIpcHandleAndSharedMemoryTypeThenInvalidArgumentIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImplicitScaling.set(0u);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;

    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    ipcData.type = static_cast<uint8_t>(ZE_MEMORY_TYPE_SHARED);

    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingOpenIpcHandlesWithIpcHandleAndHostTypeAllocationTheninvalidArgumentIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandles(device->toHandle(), numIpcHandles, ipcHandles.data(), flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingImportFdHandlesWithAllocationPointerThenAllocationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    std::vector<NEO::osHandle> handles;
    for (uint32_t i = 0; i < numIpcHandles; i++) {
        uint64_t handle = 0;
        memcpy_s(&handle,
                 sizeof(handle),
                 reinterpret_cast<void *>(ipcHandles[i].data),
                 sizeof(handle));
        handles.push_back(static_cast<NEO::osHandle>(handle));
    }

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    NEO::GraphicsAllocation *ipcAlloc = nullptr;
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(context->getDriverHandle());
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    ipcPtr = driverHandleImp->importFdHandles(device->getNEODevice(), flags, handles, nullptr, &ipcAlloc, allocDataInternal);
    EXPECT_NE(ipcPtr, nullptr);
    EXPECT_NE(ipcAlloc, nullptr);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingImportFdHandlesWithUncachedFlagAllocationPointerThenAllocationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    std::vector<NEO::osHandle> handles;
    for (uint32_t i = 0; i < numIpcHandles; i++) {
        uint64_t handle = 0;
        memcpy_s(&handle,
                 sizeof(handle),
                 reinterpret_cast<void *>(ipcHandles[i].data),
                 sizeof(handle));
        handles.push_back(static_cast<NEO::osHandle>(handle));
    }

    ze_ipc_memory_flags_t flags = {ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED};
    void *ipcPtr;
    NEO::GraphicsAllocation *ipcAlloc = nullptr;
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(context->getDriverHandle());
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    ipcPtr = driverHandleImp->importFdHandles(device->getNEODevice(), flags, handles, nullptr, &ipcAlloc, allocDataInternal);
    EXPECT_NE(ipcPtr, nullptr);
    EXPECT_NE(ipcAlloc, nullptr);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingGetIpcMemHandlesAndImportFailsThenInvalidArgumentFails) {
    currMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandles(device->toHandle(), numIpcHandles, ipcHandles.data(), flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryGetIpcHandlePidfdTest,
       givenCallToGetIpcHandleWithDeviceAllocationWithPidFdSocketMethodThenIpcHandleIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
