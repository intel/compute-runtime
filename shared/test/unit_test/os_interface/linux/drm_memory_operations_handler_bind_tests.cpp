/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <memory>

using namespace NEO;

struct MockDrmMemoryOperationsHandlerBind : public DrmMemoryOperationsHandlerBind {
    using DrmMemoryOperationsHandlerBind::DrmMemoryOperationsHandlerBind;
    using DrmMemoryOperationsHandlerBind::evictImpl;

    bool useBaseEvictUnused = true;
    uint32_t evictUnusedCalled = 0;

    MemoryOperationsStatus evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) override {
        evictUnusedCalled++;
        if (useBaseEvictUnused) {
            return DrmMemoryOperationsHandlerBind::evictUnusedAllocations(waitForCompletion, isLockNeeded);
        }

        return MemoryOperationsStatus::success;
    }
    int evictImpl(OsContext *osContext, GraphicsAllocation &gfxAllocation, DeviceBitfield deviceBitfield) override {
        EXPECT_EQ(this->rootDeviceIndex, gfxAllocation.getRootDeviceIndex());
        return DrmMemoryOperationsHandlerBind::evictImpl(osContext, gfxAllocation, deviceBitfield);
    }
};

template <uint32_t numRootDevices>
struct DrmMemoryOperationsHandlerBindFixture : public ::testing::Test {
  public:
    void setUp(bool setPerContextVms) {
        debugManager.flags.DeferOsContextInitialization.set(0);
        debugManager.flags.CreateMultipleSubDevices.set(2u);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0u; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        executionEnvironment->calculateMaxOsContextCount();
        for (uint32_t i = 0u; i < numRootDevices; i++) {
            auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);
            mock->setBindAvailable();
            if (setPerContextVms) {
                mock->setPerContextVMRequired(setPerContextVms);
                mock->incrementVmId = true;
            }
            executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[i].get(), i));
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();

            devices.emplace_back(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, i));
        }
        memoryManager = std::make_unique<TestedDrmMemoryManager>(*executionEnvironment);
        device = devices[0].get();
        mock = executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<DrmQueryMock>();
        operationHandler = static_cast<MockDrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
        memoryManagerBackup = executionEnvironment->memoryManager.release();
        executionEnvironment->memoryManager.reset(memoryManager.get());
        memoryManager->allRegisteredEngines = memoryManagerBackup->getRegisteredEngines();
    }
    void SetUp() override {
        setUp(false);
    }

    void TearDown() override {
        executionEnvironment->memoryManager.release();
        executionEnvironment->memoryManager.reset(memoryManagerBackup);
        for (auto &engineContainer : memoryManager->allRegisteredEngines) {
            engineContainer.clear();
        }
    }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    MockDevice *device;
    std::vector<std::unique_ptr<MockDevice>> devices;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    MockDrmMemoryOperationsHandlerBind *operationHandler = nullptr;
    DebugManagerStateRestore restorer;
    DrmQueryMock *mock;
    MemoryManager *memoryManagerBackup;
};

using DrmMemoryOperationsHandlerBindMultiRootDeviceTest = DrmMemoryOperationsHandlerBindFixture<2u>;

TEST_F(DrmMemoryOperationsHandlerBindMultiRootDeviceTest, whenSetNewResourceBoundToVMThenAllContextsUsingThatVMHasSetNewResourceBound) {
    struct MockOsContextLinux : OsContextLinux {
        using OsContextLinux::lastFlushedTlbFlushCounter;
    };

    BufferObject mockBo(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    mock->setNewResourceBoundToVM(&mockBo, 1u);

    for (const auto &engine : device->getAllEngines()) {
        auto osContexLinux = static_cast<MockOsContextLinux *>(engine.osContext);
        if (osContexLinux->getDeviceBitfield().test(1u) && executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]->getProductHelper().isTlbFlushRequired()) {
            EXPECT_TRUE(osContexLinux->isTlbFlushRequired());
        } else {
            EXPECT_FALSE(osContexLinux->isTlbFlushRequired());
        }

        osContexLinux->lastFlushedTlbFlushCounter.store(osContexLinux->peekTlbFlushCounter());
    }
    for (const auto &engine : devices[1]->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        EXPECT_FALSE(osContexLinux->isTlbFlushRequired());
    }

    auto mock2 = executionEnvironment->rootDeviceEnvironments[1u]->osInterface->getDriverModel()->as<DrmQueryMock>();
    BufferObject mockBo2(devices[1]->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    mock2->setNewResourceBoundToVM(&mockBo2, 0u);

    for (const auto &engine : devices[1]->getAllEngines()) {
        auto osContexLinux = static_cast<MockOsContextLinux *>(engine.osContext);
        if (osContexLinux->getDeviceBitfield().test(0u) && executionEnvironment->rootDeviceEnvironments[1]->getProductHelper().isTlbFlushRequired()) {
            EXPECT_TRUE(osContexLinux->isTlbFlushRequired());
        } else {
            EXPECT_FALSE(osContexLinux->isTlbFlushRequired());
        }

        osContexLinux->lastFlushedTlbFlushCounter.store(osContexLinux->peekTlbFlushCounter());
    }
    for (const auto &engine : device->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        EXPECT_FALSE(osContexLinux->isTlbFlushRequired());
    }

    mockBo.setAddress(0x1234);
    mock->setNewResourceBoundToVM(&mockBo, 1u);

    for (const auto &engine : device->getAllEngines()) {
        auto osContexLinux = static_cast<MockOsContextLinux *>(engine.osContext);
        if (osContexLinux->getDeviceBitfield().test(1u) && executionEnvironment->rootDeviceEnvironments[1]->getProductHelper().isTlbFlushRequired()) {
            EXPECT_TRUE(osContexLinux->isTlbFlushRequired());
        } else {
            EXPECT_FALSE(osContexLinux->isTlbFlushRequired());
        }

        osContexLinux->lastFlushedTlbFlushCounter.store(osContexLinux->peekTlbFlushCounter());
    }
    for (const auto &engine : devices[1]->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        EXPECT_FALSE(osContexLinux->isTlbFlushRequired());
    }
}

template <uint32_t numRootDevices>
struct DrmMemoryOperationsHandlerBindFixture2 : public ::testing::Test {
  public:
    void setUp(bool setPerContextVms) {
        debugManager.flags.DeferOsContextInitialization.set(0);
        debugManager.flags.CreateMultipleSubDevices.set(2u);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0u; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        executionEnvironment->calculateMaxOsContextCount();
        for (uint32_t i = 0u; i < numRootDevices; i++) {
            auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);
            mock->setBindAvailable();
            if (setPerContextVms) {
                mock->setPerContextVMRequired(setPerContextVms);
                mock->incrementVmId = true;
            }
            executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
            if (i == 0) {
                executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.reset(new DrmMemoryOperationsHandlerDefault(i));
            } else {
                executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[i].get(), i));
            }
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();

            devices.emplace_back(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, i));
        }
        memoryManager = std::make_unique<TestedDrmMemoryManager>(*executionEnvironment);
        deviceDefault = devices[0].get();
        device = devices[1].get();
        mockDefault = executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<DrmQueryMock>();
        mock = executionEnvironment->rootDeviceEnvironments[1]->osInterface->getDriverModel()->as<DrmQueryMock>();
        operationHandlerDefault = static_cast<DrmMemoryOperationsHandlerDefault *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
        operationHandler = static_cast<MockDrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[1]->memoryOperationsInterface.get());
        memoryManagerBackup = executionEnvironment->memoryManager.release();
        executionEnvironment->memoryManager.reset(memoryManager.get());
        memoryManager->allRegisteredEngines = memoryManagerBackup->getRegisteredEngines();
    }
    void SetUp() override {
        setUp(false);
    }

    void TearDown() override {
        executionEnvironment->memoryManager.release();
        executionEnvironment->memoryManager.reset(memoryManagerBackup);
        for (auto &engineContainer : memoryManager->allRegisteredEngines) {
            engineContainer.clear();
        }
    }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    MockDevice *device = nullptr;
    MockDevice *deviceDefault = nullptr;
    std::vector<std::unique_ptr<MockDevice>> devices;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMemoryOperationsHandlerDefault *operationHandlerDefault = nullptr;
    MockDrmMemoryOperationsHandlerBind *operationHandler = nullptr;
    DebugManagerStateRestore restorer;
    DrmQueryMock *mock = nullptr;
    DrmQueryMock *mockDefault = nullptr;
    MemoryManager *memoryManagerBackup = nullptr;
};

using DrmMemoryOperationsHandlerBindMultiRootDeviceTest2 = DrmMemoryOperationsHandlerBindFixture2<2u>;

TEST_F(DrmMemoryOperationsHandlerBindMultiRootDeviceTest2, givenOperationHandlersWhenRootDeviceIndexIsChangedThenEvictSucceeds) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocationDefault = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{deviceDefault->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandlerDefault->getRootDeviceIndex(), 0u);
    EXPECT_EQ(operationHandler->getRootDeviceIndex(), 1u);

    operationHandlerDefault->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocationDefault, 1), false, false);
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    operationHandlerDefault->setRootDeviceIndex(1u);
    operationHandler->setRootDeviceIndex(0u);
    EXPECT_EQ(operationHandlerDefault->getRootDeviceIndex(), 1u);
    EXPECT_EQ(operationHandler->getRootDeviceIndex(), 0u);

    EXPECT_EQ(operationHandlerDefault->evict(device, *allocation), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->evict(device, *allocationDefault), MemoryOperationsStatus::success);

    operationHandlerDefault->setRootDeviceIndex(0u);
    operationHandler->setRootDeviceIndex(1u);

    memoryManager->freeGraphicsMemory(allocationDefault);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindMultiRootDeviceTest2, whenNoSpaceLeftOnDeviceThenEvictUnusedAllocations) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocationDefault = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{deviceDefault->getRootDeviceIndex(), MemoryConstants::pageSize});
    mock->context.vmBindReturn = -1;
    mock->baseErrno = false;
    mock->errnoRetVal = ENOSPC;
    operationHandler->useBaseEvictUnused = true;

    auto registeredAllocations = memoryManager->getSysMemAllocs();
    EXPECT_EQ(2u, registeredAllocations.size());

    EXPECT_EQ(allocation, registeredAllocations[0]);
    EXPECT_EQ(allocationDefault, registeredAllocations[1]);

    EXPECT_EQ(operationHandler->evictUnusedCalled, 0u);
    auto res = operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);
    EXPECT_EQ(MemoryOperationsStatus::outOfMemory, res);
    EXPECT_EQ(operationHandler->evictUnusedCalled, 1u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocationDefault);
}
using DrmMemoryOperationsHandlerBindTest = DrmMemoryOperationsHandlerBindFixture<1u>;

TEST_F(DrmMemoryOperationsHandlerBindTest, givenObjectAlwaysResidentAndNotUsedWhenRunningOutOfMemoryThenUnusedAllocationIsNotUnbound) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto &engine : device->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(GraphicsAllocation::objectNotUsed, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(GraphicsAllocation::objectNotUsed, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    operationHandler->evictUnusedAllocations(false, true);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(mock->context.vmUnbindCalled, 1u);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenMakeEachAllocationResidentWhenCreateAllocationThenVmBindIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(1);

    EXPECT_EQ(mock->context.vmBindCalled, 0u);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.makeResident(*allocation);

    EXPECT_EQ(csr.getResidencyAllocations().size(), 0u);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenMakeEachAllocationResidentWhenMergeWithResidencyContainerThenVmBindIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(2);

    EXPECT_EQ(mock->context.vmBindCalled, 0u);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_EQ(mock->context.vmBindCalled, 0u);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    ResidencyContainer residency;
    operationHandler->mergeWithResidencyContainer(&csr.getOsContext(), residency);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    csr.makeResident(*allocation);
    EXPECT_EQ(csr.getResidencyAllocations().size(), 0u);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, whenEvictUnusedResourcesWithWaitForCompletionThenWaitCsrMethodIsCalled) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto &engine : device->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    *device->getSubDevice(1u)->getDefaultEngine().commandStreamReceiver->getTagAddress() = 5;

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount.store(123u);

    const auto status = operationHandler->evictUnusedAllocations(true, true);
    EXPECT_EQ(MemoryOperationsStatus::success, status);

    auto latestWaitTaskCount = csr.latestWaitForCompletionWithTimeoutTaskCount.load();
    EXPECT_NE(latestWaitTaskCount, 123u);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenGpuHangWhenEvictUnusedResourcesWithWaitForCompletionThenGpuHangIsReturned) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.callBaseWaitForCompletionWithTimeout = false;
    csr.returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    const auto status = operationHandler->evictUnusedAllocations(true, true);
    EXPECT_EQ(MemoryOperationsStatus::gpuHangDetectedDuringOperation, status);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenRunningOutOfMemoryThenUnusedAllocationsAreUnbound) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto &engine : device->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    *device->getSubDevice(1u)->getDefaultEngine().commandStreamReceiver->getTagAddress() = 5;

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    operationHandler->evictUnusedAllocations(false, true);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(mock->context.vmUnbindCalled, 1u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenUsedAllocationInBothSubdevicesWhenEvictUnusedThenNothingIsUnbound) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto &engine : device->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 5;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 5;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 5;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    }

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    operationHandler->evictUnusedAllocations(false, true);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(mock->context.vmUnbindCalled, 0u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenResidencyWithinOsContextFailsThenMergeWithResidencyContainertReturnsError) {
    struct MockDrmMemoryOperationsHandlerBindResidencyFail : public DrmMemoryOperationsHandlerBind {
        MockDrmMemoryOperationsHandlerBindResidencyFail(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
            : DrmMemoryOperationsHandlerBind(rootDeviceEnvironment, rootDeviceIndex) {}

        MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override {
            return NEO::MemoryOperationsStatus::failed;
        }
    };

    ResidencyContainer residencyContainer;
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBindResidencyFail(*executionEnvironment->rootDeviceEnvironments[0], 0u));
    MockDrmMemoryOperationsHandlerBindResidencyFail *operationsHandlerResidency = static_cast<MockDrmMemoryOperationsHandlerBindResidencyFail *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    auto &engines = device->getAllEngines();
    for (const auto &engine : engines) {
        EXPECT_NE(operationsHandlerResidency->mergeWithResidencyContainer(engine.osContext, residencyContainer), MemoryOperationsStatus::success);
    }
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenEvictWithinOsContextFailsThenEvictReturnsError) {
    struct MockDrmMemoryOperationsHandlerBindEvictFail : public DrmMemoryOperationsHandlerBind {
        MockDrmMemoryOperationsHandlerBindEvictFail(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
            : DrmMemoryOperationsHandlerBind(rootDeviceEnvironment, rootDeviceIndex) {}

        MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
            return NEO::MemoryOperationsStatus::failed;
        }
    };

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBindEvictFail(*executionEnvironment->rootDeviceEnvironments[0], 0u));
    MockDrmMemoryOperationsHandlerBindEvictFail *operationsHandlerEvict = static_cast<MockDrmMemoryOperationsHandlerBindEvictFail *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    EXPECT_NE(operationsHandlerEvict->evict(device, *allocation), MemoryOperationsStatus::success);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenEvictImplFailsThenEvictWithinOsContextReturnsError) {
    struct MockDrmMemoryOperationsHandlerBindEvictImplFail : public DrmMemoryOperationsHandlerBind {
        MockDrmMemoryOperationsHandlerBindEvictImplFail(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
            : DrmMemoryOperationsHandlerBind(rootDeviceEnvironment, rootDeviceIndex) {}

        int evictImpl(OsContext *osContext, GraphicsAllocation &gfxAllocation, DeviceBitfield deviceBitfield) override {
            return -1;
        }
    };

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBindEvictImplFail(*executionEnvironment->rootDeviceEnvironments[0], 0u));
    MockDrmMemoryOperationsHandlerBindEvictImplFail *operationsHandlerEvict = static_cast<MockDrmMemoryOperationsHandlerBindEvictImplFail *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
    auto &engines = device->getAllEngines();
    for (const auto &engine : engines) {
        EXPECT_NE(operationsHandlerEvict->evictWithinOsContext(engine.osContext, *allocation), MemoryOperationsStatus::success);
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenMakeBOsResidentFailsThenMakeResidentWithinOsContextReturnsError) {
    struct MockDrmAllocationBOsResident : public DrmAllocation {
        MockDrmAllocationBOsResident(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool pool)
            : DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, bos, ptrIn, gpuAddress, sizeIn, pool) {
        }

        int makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence) override {
            return -1;
        }
    };

    auto size = 1024u;
    BufferObjects bos;
    BufferObject mockBo(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    bos.push_back(&mockBo);

    auto allocation = new MockDrmAllocationBOsResident(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    auto graphicsAllocation = static_cast<GraphicsAllocation *>(allocation);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&graphicsAllocation, 1), false, false, true), MemoryOperationsStatus::outOfMemory);
    delete allocation;
}

TEST_F(DrmMemoryOperationsHandlerBindTest,
       givenDrmAllocationWithChunkingAndmakeResidentWithinOsContextCalledThenprefetchBOWithChunkingCalled) {
    struct MockDrmAllocationBOsResident : public DrmAllocation {
        MockDrmAllocationBOsResident(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool pool)
            : DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, bos, ptrIn, gpuAddress, sizeIn, pool) {
        }
    };
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(3);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    auto size = 4096u;
    BufferObjects bos;
    MockBufferObject mockBo(device->getRootDeviceIndex(), mock, 3, 0, 0, 1);
    mockBo.setChunked(true);
    mockBo.setSize(1024);
    bos.push_back(&mockBo);

    auto allocation = new MockDrmAllocationBOsResident(0, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->setNumHandles(1);
    allocation->storageInfo.isChunked = 1;
    allocation->storageInfo.numOfChunks = 4;
    allocation->storageInfo.subDeviceBitfield = 0b0011;
    auto graphicsAllocation = static_cast<GraphicsAllocation *>(allocation);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&graphicsAllocation, 1), false, false, true), MemoryOperationsStatus::success);
    delete allocation;
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenMakeResidentWithinOsContextEvictableAllocationThenAllocationIsNotMarkedAsAlwaysResident) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false, true), MemoryOperationsStatus::success);
    EXPECT_TRUE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::success);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    EXPECT_FALSE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenMakeResidentWithChunkingWithinOsContextEvictableAllocationThenAllocationIsNotMarkedAsAlwaysResident) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->storageInfo.isChunked = true;

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false, true), MemoryOperationsStatus::success);
    EXPECT_TRUE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::success);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    EXPECT_FALSE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenMakeResidentWithChunkingAndMultipleBanksWithinOsContextEvictableAllocationThenAllocationIsNotMarkedAsAlwaysResident) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->storageInfo.isChunked = true;
    allocation->storageInfo.memoryBanks = 0x5;

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false, true), MemoryOperationsStatus::success);
    EXPECT_TRUE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::success);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true, false, true), MemoryOperationsStatus::success);
    EXPECT_FALSE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenChangingResidencyThenOperationIsHandledProperly) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDeviceWithMultipleSubdevicesWhenMakeResidentWithSubdeviceThenAllocationIsBindedOnlyInItsOsContexts) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::memoryNotFound);

    auto retVal = operationHandler->makeResident(device->getSubDevice(1u), ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    EXPECT_EQ(retVal, MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::success);

    retVal = operationHandler->evict(device->getSubDevice(0u), *allocation);

    EXPECT_EQ(retVal, MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::success);

    retVal = operationHandler->evict(device->getSubDevice(1u), *allocation);

    EXPECT_EQ(retVal, MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::memoryNotFound);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenIoctlFailDuringEvictingThenUnrecoverableIsThrown) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::success);

    mock->context.vmUnbindReturn = -1;

    EXPECT_NE(operationHandler->evict(device, *allocation), MemoryOperationsStatus::success);

    mock->context.vmUnbindReturn = 0;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenMakeResidentTwiceThenAllocIsBoundOnlyOnce) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::success);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, WhenVmBindAvaialableThenMemoryManagerReturnsSupportForIndirectAllocationsAsPack) {
    mock->bindAvailable = true;
    EXPECT_TRUE(memoryManager->allowIndirectAllocationsAsPack(0u));
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenNoVmBindSupportInDrmWhenCheckForSupportThenDefaultResidencyHandlerIsReturned) {
    mock->bindAvailable = false;
    auto handler = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    mock->context.vmBindCalled = 0u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    handler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);
    EXPECT_FALSE(mock->context.vmBindCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndNoMultiTileWhenCheckForSupportThenDefaultResidencyHandlerIsReturned) {
    debugManager.flags.CreateMultipleSubDevices.set(1u);
    mock->bindAvailable = false;

    auto handler = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    mock->context.vmBindCalled = 0u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    handler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);
    EXPECT_FALSE(mock->context.vmBindCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDisabledVmBindWhenCreateDrmHandlerThenVmBindIsNotUsed) {
    mock->context.vmBindReturn = 0;
    mock->bindAvailable = false;
    auto handler = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    mock->context.vmBindCalled = false;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    handler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);
    EXPECT_FALSE(mock->context.vmBindCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenPinBOThenVmBindToAllVMsIsCalledInsteadOfExec) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    BufferObject boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenValidateHostptrThenOnlyBindToSingleVMIsCalled) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    BufferObject boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.validateHostPtr(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenValidateHostptrThenBindToGivenVm) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    BufferObject boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;
    uint32_t vmHandleId = 1u;

    pinBB.validateHostPtr(&boToPinPtr, 1u, device->getDefaultEngine().osContext, vmHandleId, 0u);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
    EXPECT_EQ(mock->context.receivedVmBind->vmId, mock->getVirtualMemoryAddressSpace(vmHandleId));
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenValidateMultipleBOsAndFirstBindFailsThenOnlyOneBindCalledAndErrorReturned) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    mock->context.vmBindReturn = -1;

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    BufferObject boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject boToPin2(device->getRootDeviceIndex(), mock, 3, 3, 0, 1);
    BufferObject *boToPinPtr[] = {&boToPin, &boToPin2};

    auto ret = pinBB.validateHostPtr(boToPinPtr, 2u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_EQ(ret, -1);

    EXPECT_EQ(mock->context.receivedVmBind->handle, 2u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

struct DrmMemoryOperationsHandlerBindWithPerContextVms : public DrmMemoryOperationsHandlerBindFixture<2> {
    void SetUp() override {
        DrmMemoryOperationsHandlerBindFixture<2>::setUp(true);
    }

    void TearDown() override {
        DrmMemoryOperationsHandlerBindFixture<2>::TearDown();
    }
};

HWTEST_F(DrmMemoryOperationsHandlerBindWithPerContextVms, givenVmBindMultipleSubdevicesAndPErContextVmsWhenValidateHostptrThenCorrectContextsVmIdIsUsed) {
    mock->bindAvailable = true;
    mock->incrementVmId = true;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(true,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    uint32_t vmIdForRootContext = 0;
    uint32_t vmIdForContext0 = 0;
    uint32_t vmIdForContext1 = 0;

    auto &engines = memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()];
    engines = EngineControlContainer{this->device->allEngines};
    engines.insert(engines.end(), this->device->getSubDevice(0)->getAllEngines().begin(), this->device->getSubDevice(0)->getAllEngines().end());
    engines.insert(engines.end(), this->device->getSubDevice(1)->getAllEngines().begin(), this->device->getSubDevice(1)->getAllEngines().end());
    for (auto &engine : engines) {
        engine.osContext->incRefInternal();
        if (engine.osContext->isDefaultContext()) {

            if (engine.osContext->getDeviceBitfield().to_ulong() == 3) {
                auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                vmIdForRootContext = osContexLinux->getDrmVmIds()[0];
            } else if (engine.osContext->getDeviceBitfield().to_ulong() == 1) {
                auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                vmIdForContext0 = osContexLinux->getDrmVmIds()[0];
            } else if (engine.osContext->getDeviceBitfield().to_ulong() == 2) {
                auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                vmIdForContext1 = osContexLinux->getDrmVmIds()[1];
            }
        }
    }

    EXPECT_NE(0u, vmIdForRootContext);
    EXPECT_NE(0u, vmIdForContext0);
    EXPECT_NE(0u, vmIdForContext1);

    AllocationData allocationData;
    allocationData.size = 13u;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    allocationData.storageInfo.subDeviceBitfield = device->getDeviceBitfield();

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(vmIdForRootContext, mock->context.receivedVmBind->vmId);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
    auto vmBindCalledBefore = mock->context.vmBindCalled;

    allocationData.storageInfo.subDeviceBitfield = device->getSubDevice(0)->getDeviceBitfield();
    allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(vmBindCalledBefore + 1, mock->context.vmBindCalled);
    EXPECT_EQ(vmIdForContext0, mock->context.receivedVmBind->vmId);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
    vmBindCalledBefore = mock->context.vmBindCalled;

    allocationData.storageInfo.subDeviceBitfield = device->getSubDevice(1)->getDeviceBitfield();
    allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(vmBindCalledBefore + 1, mock->context.vmBindCalled);
    EXPECT_EQ(vmIdForContext1, mock->context.receivedVmBind->vmId);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindWithPerContextVms, givenVmBindMultipleRootDevicesAndPerContextVmsWhenValidateHostptrThenCorrectContextsVmIdIsUsed) {
    mock->bindAvailable = true;
    mock->incrementVmId = true;

    auto device1 = devices[1].get();
    auto mock1 = executionEnvironment->rootDeviceEnvironments[1]->osInterface->getDriverModel()->as<DrmQueryMock>();
    mock1->bindAvailable = true;
    mock1->incrementVmId = true;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(true,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    uint32_t vmIdForDevice0 = 0;
    uint32_t vmIdForDevice0Subdevice0 = 0;
    uint32_t vmIdForDevice1 = 0;
    uint32_t vmIdForDevice1Subdevice0 = 0;

    {

        auto &engines = memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()];
        engines = EngineControlContainer{this->device->allEngines};
        engines.insert(engines.end(), this->device->getSubDevice(0)->getAllEngines().begin(), this->device->getSubDevice(0)->getAllEngines().end());
        engines.insert(engines.end(), this->device->getSubDevice(1)->getAllEngines().begin(), this->device->getSubDevice(1)->getAllEngines().end());
        for (auto &engine : engines) {
            engine.osContext->incRefInternal();
            if (engine.osContext->isDefaultContext()) {
                if (engine.osContext->getDeviceBitfield().to_ulong() == 3) {
                    auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                    vmIdForDevice0 = osContexLinux->getDrmVmIds()[0];
                } else if (engine.osContext->getDeviceBitfield().to_ulong() == 1) {
                    auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                    vmIdForDevice0Subdevice0 = osContexLinux->getDrmVmIds()[0];
                }
            }
        }
    }

    {
        auto &engines = memoryManager->allRegisteredEngines[device1->getRootDeviceIndex()];
        engines = EngineControlContainer{this->device->allEngines};
        engines.insert(engines.end(), device1->getSubDevice(0)->getAllEngines().begin(), device1->getSubDevice(0)->getAllEngines().end());
        engines.insert(engines.end(), device1->getSubDevice(1)->getAllEngines().begin(), device1->getSubDevice(1)->getAllEngines().end());
        for (auto &engine : engines) {
            engine.osContext->incRefInternal();
            if (engine.osContext->isDefaultContext()) {

                if (engine.osContext->getDeviceBitfield().to_ulong() == 3) {
                    auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                    vmIdForDevice1 = osContexLinux->getDrmVmIds()[0];

                } else if (engine.osContext->getDeviceBitfield().to_ulong() == 1) {
                    auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
                    vmIdForDevice1Subdevice0 = osContexLinux->getDrmVmIds()[0];
                }
            }
        }
    }
    EXPECT_NE(0u, vmIdForDevice0);
    EXPECT_NE(0u, vmIdForDevice0Subdevice0);
    EXPECT_NE(0u, vmIdForDevice1);
    EXPECT_NE(0u, vmIdForDevice1Subdevice0);

    AllocationData allocationData;
    allocationData.size = 13u;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device1->getRootDeviceIndex();
    allocationData.storageInfo.subDeviceBitfield = device1->getDeviceBitfield();

    mock->context.vmBindCalled = 0;
    mock1->context.vmBindCalled = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(mock->context.vmBindCalled, 0u);
    EXPECT_EQ(mock1->context.vmBindCalled, 1u);
    EXPECT_EQ(vmIdForDevice1, mock1->context.receivedVmBind->vmId);

    auto vmBindCalledBefore = mock1->context.vmBindCalled;

    allocationData.storageInfo.subDeviceBitfield = device->getSubDevice(0)->getDeviceBitfield();
    allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(vmBindCalledBefore + 1, mock1->context.vmBindCalled);

    EXPECT_FALSE(mock->context.receivedVmBind.has_value());
    EXPECT_EQ(vmIdForDevice1Subdevice0, mock1->context.receivedVmBind->vmId);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDirectSubmissionWhenPinBOThenVmBindIsCalledInsteadOfExec) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    device->getDefaultEngine().osContext->setDirectSubmissionActive();

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    BufferObject boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_TRUE(mock->context.vmBindCalled);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDirectSubmissionAndValidateHostptrWhenPinBOThenVmBindIsCalledInsteadOfExec) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    device->getDefaultEngine().osContext->setDirectSubmissionActive();

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    BufferObject boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.validateHostPtr(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_TRUE(mock->context.vmBindCalled);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportWhenPinBOThenAllocIsBound) {
    struct MockBO : public BufferObject {
        using BufferObject::bindInfo;
        using BufferObject::BufferObject;
    };
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    MockBO boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    auto ret = pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_TRUE(boToPin.bindInfo[0u][0u]);
    EXPECT_FALSE(ret);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportWhenPinBOAndVmBindFailedThenAllocIsNotBound) {
    struct MockBO : public BufferObject {
        using BufferObject::bindInfo;
        using BufferObject::BufferObject;
    };
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    mock->context.vmBindReturn = -1;

    BufferObject pinBB(device->getRootDeviceIndex(), mock, 3, 1, 0, 1);
    MockBO boToPin(device->getRootDeviceIndex(), mock, 3, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    auto ret = pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_FALSE(boToPin.bindInfo[0u][0u]);
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenCsrTagAllocatorsWhenDestructingCsrThenAllInternalAllocationsAreUnbound) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto timestampStorageAlloc = csr->getTimestampPacketAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    auto hwTimeStampsAlloc = csr->getEventTsAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    auto hwPerfCounterAlloc = csr->getEventPerfCountAllocator(4)->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&timestampStorageAlloc, 1), false, false);
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&hwTimeStampsAlloc, 1), false, false);
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&hwPerfCounterAlloc, 1), false, false);

    csr.reset();

    EXPECT_EQ(mock->context.vmBindCalled, mock->context.vmUnbindCalled);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenPatIndexProgrammingEnabledWhenVmBindCalledThenSetPatIndexExtension) {
    debugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    bool closSupported = (productHelper.getNumCacheRegions() > 0);
    bool patIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();

    uint64_t gpuAddress = 0x123000;
    size_t size = 1;
    BufferObject bo(0, mock, static_cast<uint64_t>(MockGmmClientContextBase::MockPatIndex::cached), 0, 1, 1);
    DrmAllocation allocation(0, 1, AllocationType::buffer, &bo, nullptr, gpuAddress, size, MemoryPool::system4KBPages);

    auto allocationPtr = static_cast<GraphicsAllocation *>(&allocation);

    for (int32_t debugFlag : {-1, 0, 1}) {
        if (debugFlag == 1 && !closSupported) {
            continue;
        }

        debugManager.flags.ClosEnabled.set(debugFlag);

        mock->context.receivedVmBindPatIndex.reset();
        mock->context.receivedVmUnbindPatIndex.reset();

        bo.setPatIndex(mock->getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, (debugFlag == 1 && closSupported), true));

        operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false);

        if (!patIndexProgrammingSupported) {
            EXPECT_FALSE(mock->context.receivedVmBindPatIndex);

            operationHandler->evict(device, allocation);
            EXPECT_FALSE(mock->context.receivedVmUnbindPatIndex);

            continue;
        }

        if (debugFlag == 0 || !closSupported || debugFlag == -1) {
            auto expectedIndex = productHelper.overridePatIndex(false, static_cast<uint64_t>(MockGmmClientContextBase::MockPatIndex::cached), allocation.getAllocationType());

            EXPECT_EQ(expectedIndex, mock->context.receivedVmBindPatIndex.value());

            operationHandler->evict(device, allocation);
            EXPECT_EQ(expectedIndex, mock->context.receivedVmUnbindPatIndex.value());
        } else {
            auto expectedPatIndex = productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeBack);
            EXPECT_EQ(expectedPatIndex, mock->context.receivedVmBindPatIndex.value());

            operationHandler->evict(device, allocation);
            EXPECT_EQ(expectedPatIndex, mock->context.receivedVmUnbindPatIndex.value());
        }
    }
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenPatIndexErrorAndUncachedDebugFlagSetWhenGetPatIndexCalledThenAbort) {
    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.ForceAllResourcesUncached.set(1);
    mock->bindAvailable = true;
    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    bool closSupported = (productHelper.getNumCacheRegions() > 0);
    bool patIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();
    if (!closSupported || !patIndexProgrammingSupported) {
        GTEST_SKIP();
    }

    static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[0]->getGmmClientContext())->returnErrorOnPatIndexQuery = true;

    uint64_t gpuAddress = 0x123000;
    size_t size = 1;
    BufferObject bo(0, mock, static_cast<uint64_t>(MockGmmClientContextBase::MockPatIndex::cached), 0, 1, 1);
    DrmAllocation allocation(0, 1, AllocationType::buffer, &bo, nullptr, gpuAddress, size, MemoryPool::system4KBPages);

    EXPECT_ANY_THROW(mock->getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false));
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenUncachedDebugFlagSetWhenVmBindCalledThenSetCorrectPatIndexExtension) {
    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.ForceAllResourcesUncached.set(1);
    mock->bindAvailable = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if (!productHelper.isVmBindPatIndexProgrammingSupported()) {
        GTEST_SKIP();
    }

    mock->context.receivedVmBindPatIndex.reset();
    mock->context.receivedVmUnbindPatIndex.reset();

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

    auto expectedIndex = productHelper.overridePatIndex(true, static_cast<uint64_t>(MockGmmClientContextBase::MockPatIndex::uncached), allocation->getAllocationType());

    EXPECT_EQ(expectedIndex, mock->context.receivedVmBindPatIndex.value());

    operationHandler->evict(device, *allocation);
    EXPECT_EQ(expectedIndex, mock->context.receivedVmUnbindPatIndex.value());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDebugFlagSetWhenVmBindCalledThenOverridePatIndex) {
    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.ClosEnabled.set(1);
    debugManager.flags.OverridePatIndex.set(1);

    mock->bindAvailable = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto timestampStorageAlloc = csr->getTimestampPacketAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if (productHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }

    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&timestampStorageAlloc, 1), false, false);

    EXPECT_EQ(1u, mock->context.receivedVmBindPatIndex.value());

    operationHandler->evict(device, *timestampStorageAlloc);

    EXPECT_EQ(1u, mock->context.receivedVmUnbindPatIndex.value());
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDebugFlagSetWhenVmBindCalledThenOverridePatIndexForDeviceMem) {
    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.ClosEnabled.set(1);
    debugManager.flags.OverridePatIndex.set(1);
    debugManager.flags.OverridePatIndexForDeviceMemory.set(2);
    debugManager.flags.OverridePatIndexForSystemMemory.set(3);

    mock->bindAvailable = true;
    mock->vmBindPatIndexProgrammingSupported = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto patIndex = mock->getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, false);
    EXPECT_EQ(2u, patIndex);

    MockBufferObject bo(0, mock, patIndex, 0, 0, 1);
    DrmAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, &bo, nullptr, 0x1234000, 1, MemoryPool::localMemory);

    GraphicsAllocation *allocPtr = &allocation;

    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false, false);

    EXPECT_EQ(2u, mock->context.receivedVmBindPatIndex.value());

    operationHandler->evict(device, allocation);

    EXPECT_EQ(2u, mock->context.receivedVmUnbindPatIndex.value());
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDebugFlagSetWhenVmBindCalledThenOverridePatIndexForSystemMem) {
    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.ClosEnabled.set(1);
    debugManager.flags.OverridePatIndex.set(1);
    debugManager.flags.OverridePatIndexForDeviceMemory.set(2);
    debugManager.flags.OverridePatIndexForSystemMemory.set(3);

    mock->bindAvailable = true;
    mock->vmBindPatIndexProgrammingSupported = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto patIndex = mock->getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true);
    EXPECT_EQ(3u, patIndex);

    MockBufferObject bo(0, mock, patIndex, 0, 0, 1);
    DrmAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, &bo, nullptr, 0x1234000, 1, MemoryPool::system4KBPages);

    GraphicsAllocation *allocPtr = &allocation;

    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false, false);

    EXPECT_EQ(3u, mock->context.receivedVmBindPatIndex.value());

    operationHandler->evict(device, allocation);

    EXPECT_EQ(3u, mock->context.receivedVmUnbindPatIndex.value());
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenClosEnabledAndAllocationToBeCachedInCacheRegionWhenVmBindIsCalledThenSetPatIndexCorrespondingToRequestedRegion) {
    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.ClosEnabled.set(1);
    mock->bindAvailable = true;

    auto csr = std::make_unique<MockCommandStreamReceiver>(*executionEnvironment, 0, 1);
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 64 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    mock->cacheInfo.reset(new CacheInfo(*mock->getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if (productHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto cacheRegion : {CacheRegion::defaultRegion, CacheRegion::region1, CacheRegion::region2}) {
        EXPECT_TRUE(static_cast<DrmAllocation *>(allocation)->setCacheAdvice(mock, 32 * MemoryConstants::kiloByte, cacheRegion, false));

        mock->context.receivedVmBindPatIndex.reset();
        operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false);

        auto patIndex = productHelper.getPatIndex(cacheRegion, CachePolicy::writeBack);

        EXPECT_EQ(patIndex, mock->context.receivedVmBindPatIndex.value());

        mock->context.receivedVmUnbindPatIndex.reset();
        operationHandler->evict(device, *allocation);

        EXPECT_EQ(patIndex, mock->context.receivedVmUnbindPatIndex.value());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenIoctlFailDuringLockingThenOutOfMemoryIsThrown) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    mock->context.vmBindReturn = -1;
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->lock(device, ArrayRef<GraphicsAllocation *>(&allocation, 1)), MemoryOperationsStatus::outOfMemory);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenLockingDrmAllocationThenBosRequireExplicitLockedMemory) {

    BufferObjects bos;
    MockBufferObject mockBo1(0, mock, 3, 0, 0, 1), mockBo2(0, mock, 3, 0, 0, 1);
    mockBo1.setSize(1024);
    mockBo2.setSize(1024);
    bos.push_back(&mockBo1);
    bos.push_back(&mockBo2);
    GraphicsAllocation *mockDrmAllocation = new MockDrmAllocation(AllocationType::unknown, MemoryPool::localMemory, bos);
    mockDrmAllocation->storageInfo.memoryBanks = 3;
    EXPECT_EQ(2u, mockDrmAllocation->storageInfo.getNumBanks());

    mock->context.vmBindReturn = 0;
    EXPECT_EQ(operationHandler->isResident(device, *mockDrmAllocation), MemoryOperationsStatus::memoryNotFound);

    EXPECT_EQ(operationHandler->lock(device, ArrayRef<GraphicsAllocation *>(&mockDrmAllocation, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(mockDrmAllocation->isLockedMemory());
    EXPECT_TRUE(mockBo1.isExplicitLockedMemoryRequired());
    EXPECT_TRUE(mockBo2.isExplicitLockedMemoryRequired());

    delete mockDrmAllocation;
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenPreviouslyLockedMemoryWhenCallingResidentMemoryThenBosDoNotRequireExplicitLockedMemory) {

    BufferObjects bos;
    MockBufferObject mockBo(0, mock, 3, 0, 0, 1);
    mockBo.setSize(1024);
    bos.push_back(&mockBo);
    GraphicsAllocation *mockDrmAllocation = new MockDrmAllocation(AllocationType::unknown, MemoryPool::localMemory, bos);

    mock->context.vmBindReturn = 0;
    mock->context.vmUnbindReturn = 0;
    EXPECT_EQ(operationHandler->isResident(device, *mockDrmAllocation), MemoryOperationsStatus::memoryNotFound);

    EXPECT_EQ(operationHandler->lock(device, ArrayRef<GraphicsAllocation *>(&mockDrmAllocation, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *mockDrmAllocation), MemoryOperationsStatus::success);

    EXPECT_EQ(operationHandler->evict(device, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *mockDrmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_FALSE(mockDrmAllocation->isLockedMemory());

    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&mockDrmAllocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *mockDrmAllocation), MemoryOperationsStatus::success);
    EXPECT_FALSE(mockDrmAllocation->isLockedMemory());
    EXPECT_FALSE(mockBo.isExplicitLockedMemoryRequired());

    delete mockDrmAllocation;
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenLockedAndResidentAllocationsWhenCallingEvictUnusedMemoryThenBothAllocationsAreNotEvicted) {
    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation1), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(operationHandler->isResident(device, *allocation2), MemoryOperationsStatus::memoryNotFound);

    EXPECT_EQ(operationHandler->lock(device, ArrayRef<GraphicsAllocation *>(&allocation1, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation1), MemoryOperationsStatus::success);

    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation2, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation2), MemoryOperationsStatus::success);

    operationHandler->useBaseEvictUnused = true;
    EXPECT_EQ(operationHandler->evictUnusedCalled, 0u);
    EXPECT_EQ(operationHandler->evictUnusedAllocations(false, true), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->evictUnusedCalled, 1u);

    EXPECT_EQ(operationHandler->isResident(device, *allocation1), MemoryOperationsStatus::success);
    EXPECT_EQ(operationHandler->isResident(device, *allocation2), MemoryOperationsStatus::success);

    memoryManager->freeGraphicsMemory(allocation2);
    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenCallingMakeResidentThenVerifyImmediateBindingIsRequired) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);

    auto bo = static_cast<DrmAllocation *>(allocation)->getBO();
    EXPECT_TRUE(bo->isImmediateBindingRequired());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenCallingEvictAfterCallingResidentThenVerifyImmediateBindingIsNotRequired) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);

    auto bo = static_cast<DrmAllocation *>(allocation)->getBO();
    EXPECT_TRUE(bo->isImmediateBindingRequired());

    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::success);
    EXPECT_FALSE(bo->isImmediateBindingRequired());

    memoryManager->freeGraphicsMemory(allocation);
}

using DrmResidencyHandlerTests = ::testing::Test;

HWTEST2_F(DrmResidencyHandlerTests, givenClosIndexAndMemoryTypeWhenAskingForPatIndexThenReturnCorrectValue, IsXeCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if (productHelper.getNumCacheRegions() == 0) {
        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::uncached));
        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeBack));
    } else {
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::uncached));
        EXPECT_EQ(1u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeCombined));
        EXPECT_EQ(2u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeThrough));
        EXPECT_EQ(3u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeBack));

        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::region1, CachePolicy::uncached));
        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::region1, CachePolicy::writeCombined));
        EXPECT_EQ(4u, productHelper.getPatIndex(CacheRegion::region1, CachePolicy::writeThrough));
        EXPECT_EQ(5u, productHelper.getPatIndex(CacheRegion::region1, CachePolicy::writeBack));

        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::region2, CachePolicy::uncached));
        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::region2, CachePolicy::writeCombined));
        EXPECT_EQ(6u, productHelper.getPatIndex(CacheRegion::region2, CachePolicy::writeThrough));
        EXPECT_EQ(7u, productHelper.getPatIndex(CacheRegion::region2, CachePolicy::writeBack));
    }
}

HWTEST2_F(DrmResidencyHandlerTests, givenForceAllResourcesUnchashedSetAskingForPatIndexThenReturnCorrectValue, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if (productHelper.getNumCacheRegions() == 0) {
        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::uncached));
        EXPECT_ANY_THROW(productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeBack));
    } else {
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::uncached));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeCombined));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeThrough));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::defaultRegion, CachePolicy::writeBack));

        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region1, CachePolicy::uncached));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region1, CachePolicy::writeCombined));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region1, CachePolicy::writeThrough));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region1, CachePolicy::writeBack));

        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region2, CachePolicy::uncached));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region2, CachePolicy::writeCombined));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region2, CachePolicy::writeThrough));
        EXPECT_EQ(0u, productHelper.getPatIndex(CacheRegion::region2, CachePolicy::writeBack));
    }
}

HWTEST2_F(DrmResidencyHandlerTests, givenSupportedVmBindAndDebugFlagUseVmBindWhenQueryingIsVmBindAvailableThenBindAvailableIsInitializedOnce, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseVmBind.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 1;
    EXPECT_FALSE(drm.bindAvailable);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_TRUE(drm.isVmBindAvailable());
    EXPECT_TRUE(drm.bindAvailable);
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);

    EXPECT_TRUE(drm.isVmBindAvailable());
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

HWTEST2_F(DrmResidencyHandlerTests, givenDebugFlagUseVmBindWhenQueryingIsVmBindAvailableThenSupportIsOverriden, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseVmBind.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    EXPECT_FALSE(drm.bindAvailable);
    drm.context.vmBindQueryReturn = -1;

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_TRUE(drm.isVmBindAvailable());
    EXPECT_TRUE(drm.bindAvailable);
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);

    EXPECT_TRUE(drm.isVmBindAvailable());
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

namespace NEO {
extern bool disableBindDefaultInTests;
}

HWTEST2_F(DrmResidencyHandlerTests, givenDebugFlagUseVmBindSetDefaultAndBindAvailableInDrmWhenQueryingIsVmBindAvailableThenBindIsAvailableWhenSupported, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseVmBind.set(-1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 1;
    drm.context.vmBindQueryReturn = 0;
    EXPECT_FALSE(drm.bindAvailable);
    auto &productHelper = drm.getRootDeviceEnvironment().getHelper<ProductHelper>();

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_EQ(drm.isVmBindAvailable(), productHelper.isNewResidencyModelSupported());
    EXPECT_EQ(drm.bindAvailable, productHelper.isNewResidencyModelSupported());
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

HWTEST2_F(DrmResidencyHandlerTests, givenDebugFlagUseVmBindSetDefaultWhenQueryingIsVmBindAvailableFailedThenBindIsNot, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseVmBind.set(-1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 1;
    drm.context.vmBindQueryReturn = -1;
    EXPECT_FALSE(drm.bindAvailable);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_FALSE(drm.isVmBindAvailable());
    EXPECT_FALSE(drm.bindAvailable);
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

HWTEST2_F(DrmResidencyHandlerTests, givenDebugFlagUseVmBindSetDefaultWhenQueryingIsVmBindAvailableSuccedAndReportNoBindAvailableInDrmThenBindIsNotAvailable, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseVmBind.set(-1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 0;
    drm.context.vmBindQueryReturn = 0;
    EXPECT_FALSE(drm.bindAvailable);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_FALSE(drm.isVmBindAvailable());
    EXPECT_FALSE(drm.bindAvailable);
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

TEST(DrmSetPairTests, whenQueryingForSetPairAvailableAndNoDebugKeyThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.setPairQueryValue = 0;
    drm.context.setPairQueryReturn = 0;
    EXPECT_FALSE(drm.setPairAvailable);

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    drm.callBaseIsSetPairAvailable = true;
    EXPECT_FALSE(drm.isSetPairAvailable());
    EXPECT_FALSE(drm.setPairAvailable);
    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
}

TEST(DrmChunkingTests, whenQueryingForChunkingAvailableAndDefaultDebugVariableThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 0;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_TRUE(drm.isChunkingAvailable());
    EXPECT_TRUE(drm.getChunkingAvailable());
    EXPECT_EQ(1u, drm.context.chunkingQueryCalled);
    EXPECT_EQ(2u, drm.getChunkingMode());
}

TEST(DrmChunkingTests, whenQueryingForChunkingAvailableAndDisableDebugVariableThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOChunking.set(0);
    debugManager.flags.UseKmdMigration.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 0;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_FALSE(drm.isChunkingAvailable());
    EXPECT_FALSE(drm.getChunkingAvailable());
    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    EXPECT_EQ(0u, drm.getChunkingMode());
}

TEST(DrmSetPairTests, whenQueryingForSetPairAvailableAndDebugKeySetAndNoSupportAvailableThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSetPair.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.setPairQueryValue = 0;
    drm.context.setPairQueryReturn = 0;
    EXPECT_FALSE(drm.setPairAvailable);

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    drm.callBaseIsSetPairAvailable = true;
    EXPECT_FALSE(drm.isSetPairAvailable());
    EXPECT_FALSE(drm.setPairAvailable);
    EXPECT_EQ(1u, drm.context.setPairQueryCalled);
}

TEST(DrmSetPairTests, whenQueryingForSetPairAvailableAndDebugKeyNotSetThenNoSupportIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSetPair.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.setPairQueryValue = 0;
    drm.context.setPairQueryReturn = 0;
    EXPECT_FALSE(drm.setPairAvailable);

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    drm.callBaseIsSetPairAvailable = true;
    EXPECT_FALSE(drm.isSetPairAvailable());
    EXPECT_FALSE(drm.setPairAvailable);
    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
}

HWTEST2_F(DrmResidencyHandlerTests, whenQueryingForSetPairAvailableAndVmBindAvailableThenBothExpectedValueIsReturned, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseVmBind.set(-1);
    debugManager.flags.EnableSetPair.set(1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &productHelper = drm.getRootDeviceEnvironment().getHelper<ProductHelper>();

    drm.context.setPairQueryValue = 1;
    drm.context.setPairQueryReturn = 0;
    EXPECT_FALSE(drm.setPairAvailable);
    drm.callBaseIsSetPairAvailable = true;

    drm.context.vmBindQueryValue = 1;
    drm.context.vmBindQueryReturn = 0;
    EXPECT_FALSE(drm.bindAvailable);
    drm.callBaseIsVmBindAvailable = true;

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    EXPECT_TRUE(drm.isSetPairAvailable());
    EXPECT_TRUE(drm.setPairAvailable);
    EXPECT_EQ(1u, drm.context.setPairQueryCalled);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_EQ(drm.isVmBindAvailable(), productHelper.isNewResidencyModelSupported());
    EXPECT_EQ(drm.bindAvailable, productHelper.isNewResidencyModelSupported());
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForSetPairAvailableAndSupportAvailableThenExpectedValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSetPair.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.setPairQueryValue = 1;
    drm.context.setPairQueryReturn = 0;
    EXPECT_FALSE(drm.setPairAvailable);

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    drm.callBaseIsSetPairAvailable = true;
    EXPECT_TRUE(drm.isSetPairAvailable());
    EXPECT_TRUE(drm.setPairAvailable);
    EXPECT_EQ(1u, drm.context.setPairQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForSetPairAvailableAndFailureInQueryThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSetPair.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.setPairQueryValue = 1;
    drm.context.setPairQueryReturn = 1;
    EXPECT_FALSE(drm.setPairAvailable);

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    drm.callBaseIsSetPairAvailable = true;
    EXPECT_FALSE(drm.isSetPairAvailable());
    EXPECT_FALSE(drm.setPairAvailable);
    EXPECT_EQ(1u, drm.context.setPairQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForSetPairAvailableWithDebugKeySetToZeroThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSetPair.set(0);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.setPairQueryValue = 1;
    drm.context.setPairQueryReturn = 1;
    EXPECT_FALSE(drm.setPairAvailable);

    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
    drm.callBaseIsSetPairAvailable = true;
    EXPECT_FALSE(drm.isSetPairAvailable());
    EXPECT_FALSE(drm.setPairAvailable);
    EXPECT_EQ(0u, drm.context.setPairQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForChunkingAvailableAndSupportAvailableThenExpectedValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.UseKmdMigration.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 0;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_TRUE(drm.isChunkingAvailable());
    EXPECT_TRUE(drm.chunkingAvailable);
    EXPECT_EQ(1u, drm.context.chunkingQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForChunkingAvailableKmdMigrationDisabledAndDefaultEnableBOChunkingThenReturnTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(0);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 0;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_TRUE(drm.isChunkingAvailable());
    EXPECT_TRUE(drm.chunkingAvailable);
    EXPECT_EQ(1u, drm.context.chunkingQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForChunkingAvailableKmdMigrationDisabledAndDefaultEnableBOChunkingSharedThenReturnFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(0);
    debugManager.flags.EnableBOChunking.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 0;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_FALSE(drm.isChunkingAvailable());
    EXPECT_FALSE(drm.chunkingAvailable);
    EXPECT_EQ(1u, drm.context.chunkingQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForChunkingAvailableAndChangingMinimalSizeForChunkingAndSupportAvailableThenExpectedValuesAreReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.UseKmdMigration.set(1);
    const uint64_t minimalSizeForChunking = 65536;
    debugManager.flags.MinimalAllocationSizeForChunking.set(minimalSizeForChunking);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 0;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_TRUE(drm.isChunkingAvailable());
    EXPECT_TRUE(drm.chunkingAvailable);
    EXPECT_EQ(1u, drm.context.chunkingQueryCalled);

    EXPECT_EQ(minimalSizeForChunking, drm.minimalChunkingSize);
}

TEST(DrmResidencyHandlerTests, whenQueryingForChunkingAvailableAndFailureInQueryThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.UseKmdMigration.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 1;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_FALSE(drm.isChunkingAvailable());
    EXPECT_FALSE(drm.chunkingAvailable);
    EXPECT_EQ(1u, drm.context.chunkingQueryCalled);
}

TEST(DrmResidencyHandlerTests, whenQueryingForChunkingAvailableWithDebugKeySetToZeroThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOChunking.set(0);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.chunkingQueryValue = 1;
    drm.context.chunkingQueryReturn = 1;
    EXPECT_FALSE(drm.chunkingAvailable);

    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
    drm.callBaseIsChunkingAvailable = true;
    EXPECT_FALSE(drm.isChunkingAvailable());
    EXPECT_FALSE(drm.chunkingAvailable);
    EXPECT_EQ(0u, drm.context.chunkingQueryCalled);
}
