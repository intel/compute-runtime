/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_driver_model.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"

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
    MemoryManagerIpcImplicitScalingObtainFdMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {}

    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; };
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override {
        return {};
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData, bool useLocalMemory) override { return nullptr; };
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
                                                                          NEO::AllocationType::BUFFER,
                                                                          nullptr,
                                                                          ptr,
                                                                          size,
                                                                          0u,
                                                                          MemoryPool::System4KBPages,
                                                                          canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        MockBufferObject bo0(&drm);
        MockBufferObject bo1(&drm);
        alloc->bufferObjects[0] = &bo0;
        alloc->bufferObjects[1] = &bo1;
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcImplicitScalingObtainFdMockGraphicsAllocation(0u,
                                                                          NEO::AllocationType::BUFFER,
                                                                          nullptr,
                                                                          ptr,
                                                                          size,
                                                                          0u,
                                                                          MemoryPool::System4KBPages,
                                                                          canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        MockBufferObject bo0(&drm);
        MockBufferObject bo1(&drm);
        alloc->bufferObjects[0] = &bo0;
        alloc->bufferObjects[1] = &bo1;
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                               AllocationProperties &properties,
                                                                               bool requireSpecificBitness,
                                                                               bool isHostIpcAllocation,
                                                                               bool reuseSharedAllocation) override {
        if (failOnCreateGraphicsAllocationFromSharedHandle) {
            return nullptr;
        }
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcImplicitScalingObtainFdMockGraphicsAllocation(0u,
                                                                          NEO::AllocationType::BUFFER,
                                                                          nullptr,
                                                                          ptr,
                                                                          size,
                                                                          0u,
                                                                          MemoryPool::System4KBPages,
                                                                          canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        MockBufferObject bo0(&drm);
        MockBufferObject bo1(&drm);
        alloc->bufferObjects[0] = &bo0;
        alloc->bufferObjects[1] = &bo1;
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
};

struct MemoryExportImportObtainFdTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.EnableImplicitScaling.set(1);

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new NEO::OSInterface());
            auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[i]);
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }

        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerIpcImplicitScalingObtainFdMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager, false);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context = std::make_unique<L0::ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto dev = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(dev->getRootDeviceIndex(), dev->toHandle()));
        }
        device = driverHandle->devices[0];
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
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
    auto eventPool = static_cast<EventPoolImp *>(EventPool::create(driverHandle.get(),
                                                                   context.get(),
                                                                   1,
                                                                   &deviceHandle,
                                                                   &eventPoolDesc,
                                                                   result));
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
    auto eventPool = static_cast<EventPoolImp *>(EventPool::create(driverHandle.get(),
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
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
    MemoryManagerIpcObtainFdMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {}

    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; };
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override {
        return {};
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData, bool useLocalMemory) override { return nullptr; };
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
                                                           NEO::AllocationType::BUFFER,
                                                           nullptr,
                                                           ptr,
                                                           size,
                                                           0u,
                                                           MemoryPool::System4KBPages,
                                                           canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        MockBufferObject bo0(&drm);
        alloc->bufferObjects[0] = &bo0;
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcObtainFdMockGraphicsAllocation(0u,
                                                           NEO::AllocationType::BUFFER,
                                                           nullptr,
                                                           ptr,
                                                           size,
                                                           0u,
                                                           MemoryPool::System4KBPages,
                                                           canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        MockBufferObject bo0(&drm);
        alloc->bufferObjects[0] = &bo0;
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }

    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                               AllocationProperties &properties,
                                                                               bool requireSpecificBitness,
                                                                               bool isHostIpcAllocation,
                                                                               bool reuseSharedAllocation) override {
        if (failOnCreateGraphicsAllocationFromSharedHandle) {
            return nullptr;
        }
        auto ptr = reinterpret_cast<void *>(sharedHandleAddress++);
        auto gmmHelper = getGmmHelper(0);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        size_t size = 0x1000;
        auto alloc = new IpcObtainFdMockGraphicsAllocation(0u,
                                                           NEO::AllocationType::BUFFER,
                                                           nullptr,
                                                           ptr,
                                                           size,
                                                           0u,
                                                           MemoryPool::System4KBPages,
                                                           canonizedGpuAddress);
        auto &drm = this->getDrm(0u);
        MockBufferObject bo0(&drm);
        alloc->bufferObjects[0] = &bo0;
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
};

struct DriverHandleObtaindFdMock : public L0::DriverHandleImp {
    void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAloc) override {
        DeviceBitfield deviceBitfield{0x0};
        AllocationProperties properties(0, MemoryConstants::pageSize,
                                        AllocationType::BUFFER,
                                        deviceBitfield);
        auto alloc = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        return reinterpret_cast<void *>(alloc->getGpuAddress());
    }
};

struct MemoryObtainFdTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new NEO::OSInterface());
            auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[i]);
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }

        driverHandle = std::make_unique<DriverHandleObtaindFdMock>();
        driverHandle->initialize(std::move(devices));

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerIpcObtainFdMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager, false);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context = std::make_unique<L0::ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto dev = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(dev->getRootDeviceIndex(), dev->toHandle()));
        }
        device = driverHandle->devices[0];
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
    EXPECT_EQ(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
