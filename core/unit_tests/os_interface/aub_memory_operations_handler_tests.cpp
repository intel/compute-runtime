/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/os_interface/aub_memory_operations_handler_tests.h"

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "unit_tests/mocks/mock_aub_manager.h"

TEST_F(AubMemoryOperationsHandlerTests, givenNullPtrAsAubManagerWhenMakeResidentCalledThenFalseReturned) {
    getMemoryOperationsHandler()->setAubManager(nullptr);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    bool result = memoryOperationsInterface->makeResident(allocation);
    EXPECT_FALSE(result);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledThenTrueReturnedAndWriteCalled) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    bool result = memoryOperationsInterface->makeResident(allocation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(aubManager.writeMemoryCalled);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAllocationWhenMakeResidentCalledThenTraceNotypeHintReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(allocation);
    EXPECT_EQ(aubManager.hintToWriteMemory, AubMemDump::DataTypeHintValues::TraceNotype);
}
TEST_F(AubMemoryOperationsHandlerTests, givenNonResidentAllocationWhenIsResidentCalledThenFalseReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    bool result = memoryOperationsInterface->isResident(allocation);
    EXPECT_FALSE(result);
}
TEST_F(AubMemoryOperationsHandlerTests, givenResidentAllocationWhenIsResidentCalledThenTrueReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(allocation);
    bool result = memoryOperationsInterface->isResident(allocation);
    EXPECT_TRUE(result);
}
TEST_F(AubMemoryOperationsHandlerTests, givenNonResidentAllocationWhenEvictCalledThenFalseReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    bool result = memoryOperationsInterface->evict(allocation);
    EXPECT_FALSE(result);
}
TEST_F(AubMemoryOperationsHandlerTests, givenResidentAllocationWhenEvictCalledThenTrueReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(allocation);
    bool result = memoryOperationsInterface->evict(allocation);
    EXPECT_TRUE(result);
}