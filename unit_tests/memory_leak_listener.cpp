/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "memory_leak_listener.h"

#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/memory_management.h"

using ::testing::TestInfo;
using namespace NEO;

extern unsigned int numBaseObjects;

void MemoryLeakListener::OnTestStart(const TestInfo &testInfo) {
    numInitialBaseObjects = numBaseObjects;
    MemoryManagement::logTraces = NEO::captureCallStacks;
    if (NEO::captureCallStacks) {
        MemoryManagement::detailedAllocationLoggingActive = true;
    }
    MemoryManagement::fastLeakDetectionEnabled = true;
}

void MemoryLeakListener::OnTestEnd(const TestInfo &testInfo) {
    MemoryManagement::fastLeakDetectionEnabled = false;
    EXPECT_EQ(numInitialBaseObjects, numBaseObjects);

    if (MemoryManagement::fastLeaksDetectionMode != MemoryManagement::LeakDetectionMode::STANDARD) {
        if (MemoryManagement::fastLeaksDetectionMode == MemoryManagement::LeakDetectionMode::EXPECT_TO_LEAK) {
            EXPECT_GT(MemoryManagement::fastEventsAllocatedCount, MemoryManagement::fastEventsDeallocatedCount);
        }
        MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::STANDARD;
    } else if (NEO::captureCallStacks && (MemoryManagement::fastEventsAllocatedCount != MemoryManagement::fastEventsDeallocatedCount)) {
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
