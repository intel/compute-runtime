/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
