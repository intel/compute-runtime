/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryManagerTest, WhenCallingIsAllocationTypeToCaptureThenScratchAndPrivateTypesReturnTrue) {
    MockMemoryManager mockMemoryManager;

    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::SCRATCH_SURFACE));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::PRIVATE_SURFACE));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::LINEAR_STREAM));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::INTERNAL_HEAP));
}

TEST(MemoryManagerTest, givenAllocationWithNullCpuPtrThenMemoryCopyToAllocationReturnsFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    constexpr uint8_t allocationSize = 10;
    uint8_t allocationStorage[allocationSize] = {0};
    MockGraphicsAllocation allocation{allocationStorage, allocationSize};
    allocation.cpuPtr = nullptr;
    constexpr size_t offset = 0;

    EXPECT_FALSE(memoryManager.copyMemoryToAllocation(&allocation, offset, nullptr, 0));
}

TEST(MemoryManagerTest, givenDefaultMemoryManagerWhenItIsAskedForBudgetExhaustionThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGettingDefaultContextThenCorrectContextForSubdeviceBitfieldIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockMemoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    auto csr0 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr2 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 3);

    csr0->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr0));
    csr1->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr1));
    csr2->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr2));

    auto osContext0 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr0.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::LowPriority}));
    auto osContext1 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}));
    auto osContext2 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}, DeviceBitfield(0x3)));
    osContext1->setDefaultContext(true);
    osContext2->setDefaultContext(true);

    EXPECT_NE(nullptr, osContext0);
    EXPECT_NE(nullptr, osContext1);
    EXPECT_NE(nullptr, osContext2);
    EXPECT_EQ(osContext1, executionEnvironment.memoryManager->getDefaultEngineContext(0, 1));
    EXPECT_EQ(osContext2, executionEnvironment.memoryManager->getDefaultEngineContext(0, 3));
    EXPECT_EQ(mockMemoryManager->getRegisteredEngines()[mockMemoryManager->defaultEngineIndex[0]].osContext,
              executionEnvironment.memoryManager->getDefaultEngineContext(0, 2));
}

TEST(MemoryManagerTest, givenMultipleDevicesMemoryManagerWhenGettingDefaultContextThenCorrectContextForDeviceAndSubdeviceBitfieldIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2);
    auto mockMemoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    auto csr0 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr2 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 3);
    auto csr3 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 1, 1);
    auto csr4 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 1, 3);

    csr0->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr0));
    csr1->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr1));
    csr2->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr2));
    csr3->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr3));
    csr4->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr4));

    auto osContext0 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr0.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::LowPriority}));
    auto osContext1 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}));
    auto osContext2 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}, DeviceBitfield(0x3)));
    osContext1->setDefaultContext(true);
    osContext2->setDefaultContext(true);

    auto osContext3 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr3.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}));
    auto osContext4 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr4.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular}, DeviceBitfield(0x3)));
    osContext3->setDefaultContext(true);
    osContext4->setDefaultContext(true);

    EXPECT_NE(nullptr, osContext0);
    EXPECT_NE(nullptr, osContext1);
    EXPECT_NE(nullptr, osContext2);
    EXPECT_NE(nullptr, osContext3);
    EXPECT_NE(nullptr, osContext4);

    EXPECT_EQ(osContext3, executionEnvironment.memoryManager->getDefaultEngineContext(1, 1));
    EXPECT_EQ(osContext4, executionEnvironment.memoryManager->getDefaultEngineContext(1, 3));
    EXPECT_EQ(mockMemoryManager->getRegisteredEngines()[mockMemoryManager->defaultEngineIndex[1]].osContext,
              executionEnvironment.memoryManager->getDefaultEngineContext(0, 2));
}

TEST(MemoryManagerTest, givenFailureOnRegisterSystemMemoryAllocationWhenAllocatingMemoryThenNullptrIsReturned) {
    AllocationProperties properties(mockRootDeviceIndex, true, MemoryConstants::cacheLineSize, AllocationType::BUFFER, false, mockDeviceBitfield);
    MockMemoryManager memoryManager;
    memoryManager.registerSysMemAllocResult = MemoryManager::AllocationStatus::Error;
    EXPECT_EQ(nullptr, memoryManager.allocateGraphicsMemoryWithProperties(properties));
}

TEST(MemoryManagerTest, givenFailureOnRegisterLocalMemoryAllocationWhenAllocatingMemoryThenNullptrIsReturned) {
    AllocationProperties properties(mockRootDeviceIndex, true, MemoryConstants::cacheLineSize, AllocationType::BUFFER, false, mockDeviceBitfield);
    MockMemoryManager memoryManager(true, true);
    memoryManager.registerLocalMemAllocResult = MemoryManager::AllocationStatus::Error;
    EXPECT_EQ(nullptr, memoryManager.allocateGraphicsMemoryWithProperties(properties));
}

using MemoryhManagerMultiContextResourceTests = ::testing::Test;
HWTEST_F(MemoryhManagerMultiContextResourceTests, givenAllocationUsedByManyOsContextsWhenCheckingUsageBeforeDestroyThenMultiContextDestructorIsUsedForWaitingForAllOsContexts) {
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), true, 2);
    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto multiContextDestructor = new MockDeferredDeleter();

    memoryManager->multiContextResourceDestructor.reset(multiContextDestructor);

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));

    auto &lowPriorityEngine = device->getEngine(device->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::LowPriority);

    auto nonDefaultOsContext = lowPriorityEngine.osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(lowPriorityEngine.commandStreamReceiver);
    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    auto defaultOsContext = device->getDefaultEngine().osContext;

    EXPECT_FALSE(defaultOsContext->isLowPriority());
    EXPECT_TRUE(nonDefaultOsContext->isLowPriority());

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto &productHelper = device->getProductHelper();
    if (productHelper.isMultiContextResourceDeferDeletionSupported()) {
        multiContextDestructor->expectClearQueueTillFirstFailure();
    } else {
        multiContextDestructor->expectDrainBlockingValue(false);
    }

    nonDefaultCsr->taskCount = *nonDefaultCsr->getTagAddress();
    nonDefaultCsr->latestFlushedTaskCount = *nonDefaultCsr->getTagAddress();
    graphicsAllocation->updateTaskCount(*nonDefaultCsr->getTagAddress(), nonDefaultOsContext->getContextId());
    graphicsAllocation->updateTaskCount(0, defaultOsContext->getContextId()); // used and ready

    EXPECT_TRUE(graphicsAllocation->isUsedByManyOsContexts());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_EQ(1, multiContextDestructor->deferDeletionCalled);
    EXPECT_TRUE(nonDefaultCsr->getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(defaultCsr->getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
}
