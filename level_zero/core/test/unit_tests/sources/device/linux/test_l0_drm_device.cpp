/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "drm_memory_manager.h"
#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using DrmDeviceTests = Test<DeviceFixture>;

TEST_F(DrmDeviceTests, whenMemoryAccessPropertiesQueriedThenConcurrentDeviceSharedMemSupportDependsOnMemoryManagerHelper) {
    constexpr auto rootDeviceIndex{0U};

    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);
    auto drm{new DrmMock{*execEnv->rootDeviceEnvironments[rootDeviceIndex]}};
    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});

    auto *origMemoryManager{device->getDriverHandle()->getMemoryManager()};
    auto *proxyMemoryManager{new TestedDrmMemoryManager{*execEnv}};
    device->getDriverHandle()->setMemoryManager(proxyMemoryManager);

    auto &productHelper = device->getProductHelper();
    ze_device_memory_access_properties_t properties;
    for (auto pfSupported : std::array{false, true}) {
        drm->pageFaultSupported = pfSupported;
        bool isKmdMigrationAvailable{proxyMemoryManager->isKmdMigrationAvailable(rootDeviceIndex)};

        proxyMemoryManager->isKmdMigrationAvailableCalled = 0U;
        auto result = device->getMemoryAccessProperties(&properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(proxyMemoryManager->isKmdMigrationAvailableCalled, 1U);

        auto expectedSharedSingleDeviceAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(productHelper.getSingleDeviceSharedMemCapabilities(isKmdMigrationAvailable));
        EXPECT_EQ(expectedSharedSingleDeviceAllocCapabilities, properties.sharedSingleDeviceAllocCapabilities);
    }

    device->getDriverHandle()->setMemoryManager(origMemoryManager);
    delete proxyMemoryManager;
}

TEST_F(DrmDeviceTests, givenDriverModelIsNotDrmWhenUsingTheApiThenUnsupportedFeatureErrorIsReturned) {
    constexpr auto rootDeviceIndex{0U};

    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);
    auto drm{new DrmMock{*execEnv->rootDeviceEnvironments[rootDeviceIndex]}};
    drm->getDriverModelTypeCallBase = false;
    drm->getDriverModelTypeResult = DriverModelType::unknown;
    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});

    EXPECT_NE(nullptr, neoDevice->getRootDeviceEnvironment().osInterface.get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(3, 0));
}

TEST_F(DrmDeviceTests, givenCacheLevelUnsupportedViaCacheReservationApiWhenUsingTheApiThenUnsupportedFeatureErrorIsReturned) {
    constexpr auto rootDeviceIndex{0U};

    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);
    auto drm{new DrmMock{*execEnv->rootDeviceEnvironments[rootDeviceIndex]}};
    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});

    EXPECT_NE(nullptr, neoDevice->getRootDeviceEnvironment().osInterface.get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(1, 0));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(2, 0));
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

class MemoryManagerFdMock : public NEO::DrmMemoryManager {
  public:
    MemoryManagerFdMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {}

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

    void freeGraphicsMemory(NEO::GraphicsAllocation *alloc, bool isImportedAllocation) override {
        delete alloc;
    }

    int obtainFdFromHandle(int boHandle, uint32_t rootDeviceIndex) override {
        if (failOnObtainFdFromHandle) {
            failOnObtainFdFromHandle = false;
            return -1;
        }
        return NEO::DrmMemoryManager::obtainFdFromHandle(boHandle, rootDeviceIndex);
    }

    bool failOnObtainFdFromHandle = false;
    uint64_t sharedHandleAddress = 0x1234;
    std::vector<std::unique_ptr<MockBufferObject>> mockBos;
};
struct MultiDeviceQueryPeerAccessDrmFixture {
    void setUp() {
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; ++i) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new OSInterface());
            auto drm = new DrmMock{*executionEnvironment->rootDeviceEnvironments[i]};
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});
        }

        driverHandle = std::make_unique<Mock<WhiteBox<L0::DriverHandleImp>>>();
        driverHandle->importFdHandleResult = reinterpret_cast<void *>(0x1234);

        for (auto &device : driverHandle->devices) {
            delete device;
        }
        driverHandle->devices.clear();

        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerFdMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        ze_context_handle_t hContext{};
        ze_context_desc_t desc{ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));

        ASSERT_EQ(numRootDevices, driverHandle->devices.size());
        device0 = driverHandle->devices[0];
        device1 = driverHandle->devices[1];
        ASSERT_NE(nullptr, device0);
        ASSERT_NE(nullptr, device1);
    }

    void tearDown() {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;

        context->destroy();
    }

    struct TempAlloc {
        NEO::Device &neoDevice;
        void *handlePtr = nullptr;
        uint64_t handle = std::numeric_limits<uint64_t>::max();

        TempAlloc(NEO::Device &device) : neoDevice(device) {}
        ~TempAlloc() {
            if (handlePtr) {
                MockDeviceImp::freeMemoryAllocation(neoDevice, handlePtr);
            }
        }
    };

    uint32_t numRootDevices = 2u;
    L0::Device *device0 = nullptr;
    L0::Device *device1 = nullptr;

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<DriverHandle>> driverHandle;
    L0::ContextImp *context = nullptr;

    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerFdMock *currMemoryManager = nullptr;

    SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
};

using MultipleDeviceQueryPeerAccessDrmTests = Test<MultiDeviceQueryPeerAccessDrmFixture>;

TEST_F(MultipleDeviceQueryPeerAccessDrmTests, givenTwoRootDevicesFromSameFamilyThenQueryPeerAccessReturnsTrue) {
    auto family0 = device0->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    auto family1 = device1->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    EXPECT_EQ(family0, family1);

    TempAlloc alloc(*device0->getNEODevice());
    bool canAccess = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &alloc.handlePtr, &alloc.handle);

    ASSERT_NE(nullptr, alloc.handlePtr);
    EXPECT_TRUE(canAccess);
}

TEST_F(MultipleDeviceQueryPeerAccessDrmTests, givenQueryPeerAccessCalledTwiceThenQueryPeerAccessReturnsTheSameValueEachTime) {
    TempAlloc alloc(*device0->getNEODevice());

    bool firstAccess = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &alloc.handlePtr, &alloc.handle);
    bool secondAccess = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &alloc.handlePtr, &alloc.handle);

    EXPECT_EQ(firstAccess, secondAccess);
}

TEST_F(MultipleDeviceQueryPeerAccessDrmTests, givenDeviceFailsAllocateMemoryThenQueryPeerAccessReturnsFalse) {
    Mock<Context> failingContext;
    failingContext.allocDeviceMemResult = ZE_RESULT_ERROR_DEVICE_LOST;

    VariableBackup<ze_context_handle_t> backupContext(&driverHandle->defaultContext, failingContext.toHandle());

    TempAlloc alloc(*device0->getNEODevice());
    bool canAccess = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &alloc.handlePtr, &alloc.handle);

    EXPECT_FALSE(canAccess);
    EXPECT_EQ(1u, failingContext.allocDeviceMemCalled);
}

TEST_F(MultipleDeviceQueryPeerAccessDrmTests, givenDeviceFailsImportFdHandleThenQueryPeerAccessReturnsFalse) {
    driverHandle->importFdHandleResult = nullptr;

    TempAlloc alloc(*device0->getNEODevice());
    bool canAccess = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &alloc.handlePtr, &alloc.handle);

    EXPECT_FALSE(canAccess);
    EXPECT_EQ(1u, driverHandle->importFdHandleCalled);
}

TEST_F(MultipleDeviceQueryPeerAccessDrmTests, givenDeviceFailPeekInternalHandleThenQueryPeerAccessReturnsFalse) {
    TempAlloc alloc(*device0->getNEODevice());

    currMemoryManager->failOnObtainFdFromHandle = true;
    bool canAccess = MockDeviceImp::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &alloc.handlePtr, &alloc.handle);

    EXPECT_FALSE(canAccess);
}

} // namespace ult
} // namespace L0