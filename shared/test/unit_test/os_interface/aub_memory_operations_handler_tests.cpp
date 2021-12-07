/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/aub_memory_operations_handler_tests.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_gmm.h"

TEST_F(AubMemoryOperationsHandlerTests, givenNullPtrAsAubManagerWhenMakeResidentCalledThenFalseReturned) {
    getMemoryOperationsHandler()->setAubManager(nullptr);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    EXPECT_EQ(result, MemoryOperationsStatus::DEVICE_UNINITIALIZED);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledThenTrueReturnedAndWriteCalled) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    EXPECT_EQ(result, MemoryOperationsStatus::SUCCESS);
    EXPECT_TRUE(aubManager.writeMemory2Called);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledOnCompressedAllocationThenPassCorrectParams) {
    MockAubManager aubManager;
    aubManager.storeAllocationParams = true;

    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmClientContext());
    gmm.isCompressionEnabled = true;
    allocPtr->setDefaultGmm(&gmm);

    auto result = memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    EXPECT_EQ(result, MemoryOperationsStatus::SUCCESS);

    EXPECT_TRUE(aubManager.writeMemory2Called);
    EXPECT_EQ(1u, aubManager.storedAllocationParams.size());
    EXPECT_TRUE(aubManager.storedAllocationParams[0].additionalParams.compressionEnabled);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAllocationWhenMakeResidentCalledThenTraceNotypeHintReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    EXPECT_EQ(aubManager.hintToWriteMemory, AubMemDump::DataTypeHintValues::TraceNotype);
}
TEST_F(AubMemoryOperationsHandlerTests, givenNonResidentAllocationWhenIsResidentCalledThenFalseReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->isResident(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::MEMORY_NOT_FOUND);
}
TEST_F(AubMemoryOperationsHandlerTests, givenResidentAllocationWhenIsResidentCalledThenTrueReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    auto result = memoryOperationsInterface->isResident(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::SUCCESS);
}
TEST_F(AubMemoryOperationsHandlerTests, givenNonResidentAllocationWhenEvictCalledThenFalseReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->evict(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::MEMORY_NOT_FOUND);
}
TEST_F(AubMemoryOperationsHandlerTests, givenResidentAllocationWhenEvictCalledThenTrueReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    auto result = memoryOperationsInterface->evict(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::SUCCESS);
}
