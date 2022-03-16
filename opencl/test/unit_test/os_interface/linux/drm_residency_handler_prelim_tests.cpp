/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/cache_info_impl.h"
#include "shared/source/os_interface/linux/clos_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

using namespace NEO;

struct MockDrmMemoryOperationsHandlerBind : public DrmMemoryOperationsHandlerBind {
    using DrmMemoryOperationsHandlerBind::DrmMemoryOperationsHandlerBind;
    using DrmMemoryOperationsHandlerBind::evictImpl;

    bool useBaseEvictUnused = true;
    uint32_t evictUnusedCalled = 0;

    void evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) override {
        evictUnusedCalled++;
        if (useBaseEvictUnused) {
            DrmMemoryOperationsHandlerBind::evictUnusedAllocations(waitForCompletion, isLockNeeded);
        }
    }
};

template <uint32_t numRootDevices>
struct DrmMemoryOperationsHandlerBindFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.DeferOsContextInitialization.set(0);
        DebugManager.flags.CreateMultipleSubDevices.set(2u);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0u; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        }
        executionEnvironment->calculateMaxOsContextCount();
        for (uint32_t i = 0u; i < numRootDevices; i++) {
            auto mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[i]);
            mock->setBindAvailable();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[i].get(), i));

            devices.emplace_back(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, i));
        }
        memoryManager = std::make_unique<TestedDrmMemoryManager>(*executionEnvironment);
        device = devices[0].get();
        mock = executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<DrmQueryMock>();
        operationHandler = static_cast<MockDrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
        memoryManagerBackup = executionEnvironment->memoryManager.release();
        executionEnvironment->memoryManager.reset(memoryManager.get());
        memoryManager->registeredEngines = memoryManagerBackup->getRegisteredEngines();
    }

    void TearDown() override {
        executionEnvironment->memoryManager.release();
        executionEnvironment->memoryManager.reset(memoryManagerBackup);
        memoryManager->getRegisteredEngines().clear();
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
    mock->setNewResourceBoundToVM(1u);

    for (const auto &engine : device->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        if (osContexLinux->getDeviceBitfield().test(1u)) {
            EXPECT_TRUE(osContexLinux->getNewResourceBound());
        } else {
            EXPECT_FALSE(osContexLinux->getNewResourceBound());
        }

        osContexLinux->setNewResourceBound(false);
    }
    for (const auto &engine : devices[1]->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        EXPECT_FALSE(osContexLinux->getNewResourceBound());
    }

    auto mock2 = executionEnvironment->rootDeviceEnvironments[1u]->osInterface->getDriverModel()->as<DrmQueryMock>();
    mock2->setNewResourceBoundToVM(0u);

    for (const auto &engine : devices[1]->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        if (osContexLinux->getDeviceBitfield().test(0u)) {
            EXPECT_TRUE(osContexLinux->getNewResourceBound());
        } else {
            EXPECT_FALSE(osContexLinux->getNewResourceBound());
        }
    }
    for (const auto &engine : device->getAllEngines()) {
        auto osContexLinux = static_cast<OsContextLinux *>(engine.osContext);
        EXPECT_FALSE(osContexLinux->getNewResourceBound());
    }
}

using DrmMemoryOperationsHandlerBindTest = DrmMemoryOperationsHandlerBindFixture<1u>;

TEST_F(DrmMemoryOperationsHandlerBindTest, whenNoSpaceLeftOnDeviceThenEvictUnusedAllocations) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    mock->context.vmBindReturn = -1;
    mock->baseErrno = false;
    mock->errnoRetVal = ENOSPC;
    operationHandler->useBaseEvictUnused = false;

    EXPECT_EQ(operationHandler->evictUnusedCalled, 0u);
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1));
    EXPECT_EQ(operationHandler->evictUnusedCalled, 1u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenObjectAlwaysResidentAndNotUsedWhenRunningOutOfMemoryThenUnusedAllocationIsNotUnbound) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto &engine : device->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(GraphicsAllocation::objectNotUsed, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(GraphicsAllocation::objectNotUsed, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    operationHandler->evictUnusedAllocations(false, true);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(mock->context.vmUnbindCalled, 1u);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenMakeEachAllocationResidentWhenCreateAllocationThenVmBindIsCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.MakeEachAllocationResident.set(1);

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
    DebugManager.flags.MakeEachAllocationResident.set(2);

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
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    *device->getSubDevice(1u)->getDefaultEngine().commandStreamReceiver->getTagAddress() = 5;

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount.store(123u);

    operationHandler->evictUnusedAllocations(true, true);

    auto latestWaitTaskCount = csr.latestWaitForCompletionWithTimeoutTaskCount.load();
    EXPECT_NE(latestWaitTaskCount, 123u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenRunningOutOfMemoryThenUnusedAllocationsAreUnbound) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto &engine : device->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 10;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
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
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(0u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 5;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }
    for (auto &engine : device->getSubDevice(1u)->getAllEngines()) {
        *engine.commandStreamReceiver->getTagAddress() = 5;
        allocation->updateTaskCount(8u, engine.osContext->getContextId());
        EXPECT_EQ(operationHandler->makeResidentWithinOsContext(engine.osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    }

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    operationHandler->evictUnusedAllocations(false, true);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(mock->context.vmUnbindCalled, 0u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenResidencyWithinOsContextFailsThenThenMergeWithResidencyContainertReturnsError) {
    struct MockDrmMemoryOperationsHandlerBindResidencyFail : public DrmMemoryOperationsHandlerBind {
        MockDrmMemoryOperationsHandlerBindResidencyFail(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
            : DrmMemoryOperationsHandlerBind(rootDeviceEnvironment, rootDeviceIndex) {}

        MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override {
            return NEO::MemoryOperationsStatus::FAILED;
        }
    };

    ResidencyContainer residencyContainer;
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBindResidencyFail(*executionEnvironment->rootDeviceEnvironments[0], 0u));
    MockDrmMemoryOperationsHandlerBindResidencyFail *operationsHandlerResidency = static_cast<MockDrmMemoryOperationsHandlerBindResidencyFail *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    auto &engines = device->getAllEngines();
    for (const auto &engine : engines) {
        EXPECT_NE(operationsHandlerResidency->mergeWithResidencyContainer(engine.osContext, residencyContainer), MemoryOperationsStatus::SUCCESS);
    }
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenEvictWithinOsContextFailsThenEvictReturnsError) {
    struct MockDrmMemoryOperationsHandlerBindEvictFail : public DrmMemoryOperationsHandlerBind {
        MockDrmMemoryOperationsHandlerBindEvictFail(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
            : DrmMemoryOperationsHandlerBind(rootDeviceEnvironment, rootDeviceIndex) {}

        MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
            return NEO::MemoryOperationsStatus::FAILED;
        }
    };

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(new MockDrmMemoryOperationsHandlerBindEvictFail(*executionEnvironment->rootDeviceEnvironments[0], 0u));
    MockDrmMemoryOperationsHandlerBindEvictFail *operationsHandlerEvict = static_cast<MockDrmMemoryOperationsHandlerBindEvictFail *>(executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.get());

    EXPECT_NE(operationsHandlerEvict->evict(device, *allocation), MemoryOperationsStatus::SUCCESS);

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
        EXPECT_NE(operationsHandlerEvict->evictWithinOsContext(engine.osContext, *allocation), MemoryOperationsStatus::SUCCESS);
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenMakeBOsResidentFailsThenMakeResidentWithinOsContextReturnsError) {
    struct MockDrmAllocationBOsResident : public DrmAllocation {
        MockDrmAllocationBOsResident(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
            : DrmAllocation(rootDeviceIndex, allocationType, bos, ptrIn, gpuAddress, sizeIn, pool) {
        }

        int makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) override {
            return -1;
        }
    };

    auto size = 1024u;
    BufferObjects bos;

    auto allocation = new MockDrmAllocationBOsResident(0, AllocationType::UNKNOWN, bos, nullptr, 0u, size, MemoryPool::LocalMemory);
    auto graphicsAllocation = static_cast<GraphicsAllocation *>(allocation);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&graphicsAllocation, 1), false), MemoryOperationsStatus::OUT_OF_MEMORY);
    delete allocation;
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenMakeResidentWithinOsContextEvictableAllocationThenAllocationIsNotMarkedAsAlwaysResident) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), false), MemoryOperationsStatus::SUCCESS);
    EXPECT_TRUE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::SUCCESS);

    EXPECT_EQ(operationHandler->makeResidentWithinOsContext(device->getDefaultEngine().osContext, ArrayRef<GraphicsAllocation *>(&allocation, 1), true), MemoryOperationsStatus::SUCCESS);
    EXPECT_FALSE(allocation->isAlwaysResident(device->getDefaultEngine().osContext->getContextId()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDrmMemoryOperationBindWhenChangingResidencyThenOperationIsHandledProperly) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->evict(device, *allocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDeviceWithMultipleSubdevicesWhenMakeResidentWithSubdeviceThenAllocationIsBindedOnlyInItsOsContexts) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);

    auto retVal = operationHandler->makeResident(device->getSubDevice(1u), ArrayRef<GraphicsAllocation *>(&allocation, 1));

    EXPECT_EQ(retVal, MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::SUCCESS);

    retVal = operationHandler->evict(device->getSubDevice(0u), *allocation);

    EXPECT_EQ(retVal, MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::SUCCESS);

    retVal = operationHandler->evict(device->getSubDevice(1u), *allocation);

    EXPECT_EQ(retVal, MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(0u), *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->isResident(device->getSubDevice(1u), *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenIoctlFailDuringEvictingThenUnrecoverableIsThrown) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::SUCCESS);

    mock->context.vmUnbindReturn = -1;

    EXPECT_NE(operationHandler->evict(device, *allocation), MemoryOperationsStatus::SUCCESS);

    mock->context.vmUnbindReturn = 0;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, whenMakeResidentTwiceThenAllocIsBoundOnlyOnce) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(operationHandler->isResident(device, *allocation), MemoryOperationsStatus::SUCCESS);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, WhenVmBindAvaialableThenMemoryManagerReturnsSupportForIndirectAllocationsAsPack) {
    mock->bindAvailable = true;
    EXPECT_TRUE(memoryManager->allowIndirectAllocationsAsPack(0u));
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenNoVmBindSupportInDrmWhenCheckForSupportThenDefaultResidencyHandlerIsReturned) {
    mock->bindAvailable = false;
    auto handler = DrmMemoryOperationsHandler::create(*mock, 0u);

    mock->context.vmBindCalled = 0u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    handler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1));
    EXPECT_FALSE(mock->context.vmBindCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndNoMultiTileWhenCheckForSupportThenDefaultResidencyHandlerIsReturned) {
    DebugManager.flags.CreateMultipleSubDevices.set(1u);
    mock->bindAvailable = false;

    auto handler = DrmMemoryOperationsHandler::create(*mock, 0u);

    mock->context.vmBindCalled = 0u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    handler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1));
    EXPECT_FALSE(mock->context.vmBindCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenDisabledVmBindWhenCreateDrmHandlerThenVmBindIsNotUsed) {
    mock->context.vmBindReturn = 0;
    mock->bindAvailable = false;
    auto handler = DrmMemoryOperationsHandler::create(*mock, 0u);

    mock->context.vmBindCalled = false;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    handler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1));
    EXPECT_FALSE(mock->context.vmBindCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenPinBOThenVmBindToAllVMsIsCalledInsteadOfExec) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(mock, 1, 0, 1);
    BufferObject boToPin(mock, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_EQ(mock->context.vmBindCalled, 2u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenValidateHostptrThenOnlyBindToSingleVMIsCalled) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(mock, 1, 0, 1);
    BufferObject boToPin(mock, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.validateHostPtr(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenValidateHostptrThenBindToGivenVm) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(mock, 1, 0, 1);
    BufferObject boToPin(mock, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;
    uint32_t vmHandleId = 1u;

    pinBB.validateHostPtr(&boToPinPtr, 1u, device->getDefaultEngine().osContext, vmHandleId, 0u);

    EXPECT_EQ(mock->context.vmBindCalled, 1u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
    EXPECT_EQ(mock->context.receivedVmBind->vmId, mock->getVirtualMemoryAddressSpace(vmHandleId));
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenVmBindSupportAndMultiSubdeviceWhenValidateMultipleBOsAndFirstBindFailsThenOnlyOneBindCalledAndErrorReturned) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    mock->context.vmBindReturn = -1;

    BufferObject pinBB(mock, 1, 0, 1);
    BufferObject boToPin(mock, 2, 0, 1);
    BufferObject boToPin2(mock, 3, 0, 1);
    BufferObject *boToPinPtr[] = {&boToPin, &boToPin2};

    auto ret = pinBB.validateHostPtr(boToPinPtr, 2u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_EQ(ret, -1);

    EXPECT_EQ(mock->context.receivedVmBind->handle, 2u);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDirectSubmissionWhenPinBOThenVmBindIsCalledInsteadOfExec) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    device->getDefaultEngine().osContext->setDirectSubmissionActive();

    BufferObject pinBB(mock, 1, 0, 1);
    BufferObject boToPin(mock, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_TRUE(mock->context.vmBindCalled);
    EXPECT_EQ(0, mock->ioctlCount.execbuffer2);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDirectSubmissionAndValidateHostptrWhenPinBOThenVmBindIsCalledInsteadOfExec) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    device->getDefaultEngine().osContext->setDirectSubmissionActive();

    BufferObject pinBB(mock, 1, 0, 1);
    BufferObject boToPin(mock, 2, 0, 1);
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
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    BufferObject pinBB(mock, 1, 0, 1);
    MockBO boToPin(mock, 2, 0, 1);
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
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    mock->context.vmBindReturn = -1;

    BufferObject pinBB(mock, 1, 0, 1);
    MockBO boToPin(mock, 2, 0, 1);
    BufferObject *boToPinPtr = &boToPin;

    auto ret = pinBB.pin(&boToPinPtr, 1u, device->getDefaultEngine().osContext, 0u, 0u);

    EXPECT_FALSE(boToPin.bindInfo[0u][0u]);
    EXPECT_TRUE(ret);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenCsrTagAllocatorsWhenDestructingCsrThenAllInternalAllocationsAreUnbound) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;
    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto timestampStorageAlloc = csr->getTimestampPacketAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    auto hwTimeStampsAlloc = csr->getEventTsAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    auto hwPerfCounterAlloc = csr->getEventPerfCountAllocator(4)->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&timestampStorageAlloc, 1));
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&hwTimeStampsAlloc, 1));
    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&hwPerfCounterAlloc, 1));

    csr.reset();

    EXPECT_EQ(mock->context.vmBindCalled, mock->context.vmUnbindCalled);
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenClosEnabledWhenVmBindCalledThenSetPatIndexExtension) {
    DebugManager.flags.UseVmBind.set(1);
    mock->bindAvailable = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto timestampStorageAlloc = csr->getTimestampPacketAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto &hwHelper = HwHelper::get(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eRenderCoreFamily);
    bool supported = (hwHelper.getNumCacheRegions() > 0);

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.ClosEnabled.set(debugFlag);

        mock->context.receivedVmBindPatIndex.reset();
        operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&timestampStorageAlloc, 1));

        if (debugFlag == 0 || (debugFlag == -1 && !supported)) {
            EXPECT_FALSE(mock->context.receivedVmBindPatIndex);

            operationHandler->evict(device, *timestampStorageAlloc);
            EXPECT_FALSE(mock->context.receivedVmUnbindPatIndex);

            continue;
        }

        EXPECT_EQ(3u, mock->context.receivedVmBindPatIndex.value());

        mock->context.receivedVmUnbindPatIndex.reset();
        operationHandler->evict(device, *timestampStorageAlloc);

        EXPECT_EQ(3u, mock->context.receivedVmUnbindPatIndex.value());

        mock->context.receivedVmBindPatIndex.reset();
        mock->context.receivedVmUnbindPatIndex.reset();
    }
}

HWTEST_F(DrmMemoryOperationsHandlerBindTest, givenDebugFlagSetWhenVmBindCalledThenOverridePatIndex) {
    DebugManager.flags.UseVmBind.set(1);
    DebugManager.flags.ClosEnabled.set(1);
    DebugManager.flags.OverridePatIndex.set(1);

    mock->bindAvailable = true;

    auto csr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, DeviceBitfield(1));
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto timestampStorageAlloc = csr->getTimestampPacketAllocator()->getTag()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();

    auto &hwHelper = HwHelper::get(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eRenderCoreFamily);

    if (hwHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }

    operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&timestampStorageAlloc, 1));

    EXPECT_EQ(1u, mock->context.receivedVmBindPatIndex.value());

    operationHandler->evict(device, *timestampStorageAlloc);

    EXPECT_EQ(1u, mock->context.receivedVmUnbindPatIndex.value());
}

TEST_F(DrmMemoryOperationsHandlerBindTest, givenClosEnabledAndAllocationToBeCachedInCacheRegionWhenVmBindIsCalledThenSetPatIndexCorrespondingToRequestedRegion) {
    DebugManager.flags.UseVmBind.set(1);
    DebugManager.flags.ClosEnabled.set(1);
    mock->bindAvailable = true;

    auto csr = std::make_unique<MockCommandStreamReceiver>(*executionEnvironment, 0, 1);
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    mock->cacheInfo.reset(new CacheInfoImpl(*mock, 64 * MemoryConstants::kiloByte, 2, 32));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    for (auto cacheRegion : {CacheRegion::Default, CacheRegion::Region1, CacheRegion::Region2}) {
        EXPECT_TRUE(static_cast<DrmAllocation *>(allocation)->setCacheAdvice(mock, 32 * MemoryConstants::kiloByte, cacheRegion));

        mock->context.receivedVmBindPatIndex.reset();
        operationHandler->makeResident(device, ArrayRef<GraphicsAllocation *>(&allocation, 1));

        EXPECT_EQ(ClosHelper::getPatIndex(cacheRegion, CachePolicy::WriteBack), mock->context.receivedVmBindPatIndex.value());

        mock->context.receivedVmUnbindPatIndex.reset();
        operationHandler->evict(device, *allocation);

        EXPECT_EQ(ClosHelper::getPatIndex(cacheRegion, CachePolicy::WriteBack), mock->context.receivedVmUnbindPatIndex.value());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmResidencyHandlerTests, givenClosIndexAndMemoryTypeWhenAskingForPatIndexThenReturnCorrectValue) {
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::Uncached));
    EXPECT_EQ(1u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::WriteCombined));
    EXPECT_EQ(2u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::WriteThrough));
    EXPECT_EQ(3u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::WriteBack));

    EXPECT_ANY_THROW(ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::Uncached));
    EXPECT_ANY_THROW(ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::WriteCombined));
    EXPECT_EQ(4u, ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::WriteThrough));
    EXPECT_EQ(5u, ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::WriteBack));

    EXPECT_ANY_THROW(ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::Uncached));
    EXPECT_ANY_THROW(ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::WriteCombined));
    EXPECT_EQ(6u, ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::WriteThrough));
    EXPECT_EQ(7u, ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::WriteBack));
}

TEST(DrmResidencyHandlerTests, givenForceAllResourcesUnchashedSetAskingForPatIndexThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceAllResourcesUncached.set(1);

    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::Uncached));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::WriteCombined));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::WriteThrough));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Default, CachePolicy::WriteBack));

    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::Uncached));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::WriteCombined));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::WriteThrough));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region1, CachePolicy::WriteBack));

    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::Uncached));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::WriteCombined));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::WriteThrough));
    EXPECT_EQ(0u, ClosHelper::getPatIndex(CacheRegion::Region2, CachePolicy::WriteBack));
}

TEST(DrmResidencyHandlerTests, givenSupportedVmBindAndDebugFlagUseVmBindWhenQueryingIsVmBindAvailableThenBindAvailableIsInitializedOnce) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseVmBind.set(1);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
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

TEST(DrmResidencyHandlerTests, givenDebugFlagUseVmBindWhenQueryingIsVmBindAvailableThenSupportIsOverriden) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseVmBind.set(1);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
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

TEST(DrmResidencyHandlerTests, givenDebugFlagUseVmBindSetDefaultAndBindAvailableInDrmWhenQueryingIsVmBindAvailableThenBindIsAvailableWhenSupported) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseVmBind.set(-1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 1;
    drm.context.vmBindQueryReturn = 0;
    EXPECT_FALSE(drm.bindAvailable);

    auto hwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();
    auto hwInfoConfig = HwInfoConfig::get(hwInfo->platform.eProductFamily);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_EQ(drm.isVmBindAvailable(), hwInfoConfig->isNewResidencyModelSupported());
    EXPECT_EQ(drm.bindAvailable, hwInfoConfig->isNewResidencyModelSupported());
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

TEST(DrmResidencyHandlerTests, givenDebugFlagUseVmBindSetDefaultWhenQueryingIsVmBindAvailableFailedThenBindIsNot) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseVmBind.set(-1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 1;
    drm.context.vmBindQueryReturn = -1;
    EXPECT_FALSE(drm.bindAvailable);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_FALSE(drm.isVmBindAvailable());
    EXPECT_FALSE(drm.bindAvailable);
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}

TEST(DrmResidencyHandlerTests, givenDebugFlagUseVmBindSetDefaultWhenQueryingIsVmBindAvailableSuccedAndReportNoBindAvailableInDrmThenBindIsNotAvailable) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseVmBind.set(-1);
    VariableBackup<bool> disableBindBackup(&disableBindDefaultInTests, false);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.context.vmBindQueryValue = 0;
    drm.context.vmBindQueryReturn = 0;
    EXPECT_FALSE(drm.bindAvailable);

    EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
    EXPECT_FALSE(drm.isVmBindAvailable());
    EXPECT_FALSE(drm.bindAvailable);
    EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
}
