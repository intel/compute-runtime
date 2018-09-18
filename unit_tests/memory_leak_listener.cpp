/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "memory_leak_listener.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/fixtures/memory_management_fixture.h"

using ::testing::TestInfo;
using namespace OCLRT;

extern unsigned int numBaseObjects;

void MemoryLeakListener::OnTestStart(const TestInfo &testInfo) {
    numInitialBaseObjects = numBaseObjects;
    MemoryManagement::logTraces = OCLRT::captureCallStacks;
    if (OCLRT::captureCallStacks) {
        MemoryManagement::detailedAllocationLoggingActive = true;
    }
    MemoryManagement::fastLeakDetectionMode = 1;
}

void MemoryLeakListener::OnTestEnd(const TestInfo &testInfo) {
    MemoryManagement::fastLeakDetectionMode = 0;
    EXPECT_EQ(numInitialBaseObjects, numBaseObjects);

    if (MemoryManagement::fastLeaksDetectionMode != MemoryManagement::LeakDetectionMode::STANDARD) {
        if (MemoryManagement::fastLeaksDetectionMode == MemoryManagement::LeakDetectionMode::EXPECT_TO_LEAK) {
            EXPECT_GT(MemoryManagement::fastEventsAllocatedCount, MemoryManagement::fastEventsDeallocatedCount);
        }
        MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::STANDARD;
    } else if (OCLRT::captureCallStacks && (MemoryManagement::fastEventsAllocatedCount != MemoryManagement::fastEventsDeallocatedCount)) {
        auto leak = MemoryManagementFixture::enumerateLeak(MemoryManagement::indexAllocation.load(), MemoryManagement::indexDeallocation.load(), true, true);
        if (leak != MemoryManagement::failingAllocation) {
            printf("\n %s ", printCallStack(MemoryManagement::eventsAllocated[leak]).c_str());
        }
        EXPECT_EQ(MemoryManagement::indexAllocation.load(), MemoryManagement::indexDeallocation.load());
    } else if (MemoryManagement::fastEventsAllocatedCount != MemoryManagement::fastEventsDeallocatedCount) {
        auto leak = MemoryManagement::detectLeaks();
        EXPECT_EQ(leak, (int)MemoryManagement::failingAllocation) << "To locate call stack, change the value of captureCallStacks to true";
    }
    MemoryManagement::fastEventsAllocatedCount = 0;
    MemoryManagement::fastEventsDeallocatedCount = 0;
}
