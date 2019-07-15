/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/memory_management_fixture.h"

#include "core/unit_tests/helpers/memory_leak_listener.h"
#include "core/unit_tests/helpers/memory_management.h"
#include "runtime/helpers/options.h"

#include <cinttypes>
#if defined(__linux__)
#include <cstdio>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#elif defined(_WIN32)
#include <Windows.h>
#pragma warning(push)           // Saves the current warning state.
#pragma warning(disable : 4091) // Temporarily disables warning 4091.
#include <DbgHelp.h>
#pragma warning(pop) // Restores the warning state.
#pragma comment(lib, "Dbghelp.lib")
#endif

namespace Os {
extern const char *frontEndDllName;
extern const char *igcDllName;
} // namespace Os

void MemoryManagementFixture::SetUp() {
    EXPECT_EQ(static_cast<size_t>(-1), MemoryManagement::failingAllocation);
    MemoryManagement::indexAllocation = 0;
    MemoryManagement::indexDeallocation = 0;
    MemoryManagement::failingAllocation = -1;
    previousAllocations = MemoryManagement::numAllocations.load();
    MemoryManagement::logTraces = MemoryManagement::captureCallStacks;
}

void MemoryManagementFixture::TearDown() {
    clearFailingAllocation();
    checkForLeaks();
    MemoryManagement::logTraces = false;
}

void MemoryManagementFixture::setFailingAllocation(size_t allocation) {
    MemoryManagement::indexAllocation = 0;
    MemoryManagement::failingAllocation = allocation;
}

void MemoryManagementFixture::clearFailingAllocation() {
    MemoryManagement::failingAllocation = -1;
}

::testing::AssertionResult MemoryManagementFixture::assertLeak(
    const char *leakExpr,
    size_t leakIndex) {
    using MemoryManagement::AllocationEvent;
    using MemoryManagement::eventsAllocated;

    if (leakIndex == MemoryManagement::invalidLeakIndex) {
        return ::testing::AssertionSuccess();
    }
    auto &event = eventsAllocated[leakIndex];

    switch (event.event) {
    case AllocationEvent::EVENT_DELETE:
        return ::testing::AssertionFailure() << "event[" << leakIndex
                                             << "]: delete doesn't have corresponding new. allocation address="
                                             << event.address
                                             << ", allocation size=" << event.size << printCallStack(event);
        break;
    case AllocationEvent::EVENT_DELETE_ARRAY:
        return ::testing::AssertionFailure() << "event[" << leakIndex
                                             << "]: delete[] doesn't have corresponding new[]. allocation address="
                                             << event.address
                                             << ", allocation size=" << event.size << printCallStack(event);
        break;
    case AllocationEvent::EVENT_NEW:
        return ::testing::AssertionFailure() << "event[" << leakIndex
                                             << "]: new doesn't have corresponding delete. allocation address="
                                             << event.address
                                             << ", allocation size=" << event.size << printCallStack(event);
        break;
    case AllocationEvent::EVENT_NEW_NOTHROW:
        return ::testing::AssertionFailure() << "event[" << leakIndex
                                             << "]: new (std::nothrow) doesn't have corresponding delete. allocation address="
                                             << event.address
                                             << ", allocation size=" << event.size << printCallStack(event);
        break;
    case AllocationEvent::EVENT_NEW_ARRAY:
        return ::testing::AssertionFailure() << "event[" << leakIndex
                                             << "]: new [] doesn't have corresponding delete[]. allocation address="
                                             << event.address
                                             << ", allocation size=" << event.size << printCallStack(event);
        break;
    case AllocationEvent::EVENT_NEW_ARRAY_NOTHROW:
        return ::testing::AssertionFailure() << "event[" << leakIndex
                                             << "] new (std::nothrow) [] doesn't have corresponding delete[]. allocation address="
                                             << event.address
                                             << ", allocation size=" << event.size << printCallStack(event);
        break;
    case AllocationEvent::EVENT_NEW_ARRAY_NOTHROW_FAIL:
    case AllocationEvent::EVENT_NEW_ARRAY_FAIL:
    case AllocationEvent::EVENT_NEW_NOTHROW_FAIL:
    case AllocationEvent::EVENT_NEW_FAIL:
    case AllocationEvent::EVENT_UNKNOWN:
    default:
        return ::testing::AssertionFailure() << "Unknown event[" << leakIndex << "] detected. allocation size=" << event.size;
        break;
    }
}

void MemoryManagementFixture::checkForLeaks() {
    // We have to alias MemoryManagement::numAllocations because
    // the following EXPECT_EQ actually allocates more memory :-)
    auto currentAllocations = MemoryManagement::numAllocations.load();
    auto indexAllocationTop = MemoryManagement::indexAllocation.load();
    auto indexDellocationTop = MemoryManagement::indexDeallocation.load();
    if (previousAllocations != currentAllocations) {
        auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        auto testResult = testInfo->result();
        if (testResult->Passed()) {
            //EXPECT_EQ(previousAllocations, currentAllocations);
            size_t leakEventIndex;
            do {
                leakEventIndex = MemoryManagement::enumerateLeak(indexAllocationTop, indexDellocationTop, false, false);
                EXPECT_PRED_FORMAT1(assertLeak, leakEventIndex);
                auto invalidLeakIndexValues = MemoryManagement::invalidLeakIndex;
                EXPECT_EQ(leakEventIndex, invalidLeakIndexValues);
            } while (leakEventIndex != MemoryManagement::invalidLeakIndex);
        } else {
            printf("*** WARNING: Leaks found but dumping disabled during test failure ***\n");
        }
    }
}

void MemoryManagementFixture::injectFailures(InjectedFunction &method, uint32_t maxIndex) {
    MemoryManagement::indexAllocation = 0;
    method(-1);
    auto numCurrentAllocations = MemoryManagement::indexAllocation.load();

    for (auto i = 0u; i < numCurrentAllocations; i++) {
        // Force a failure
        MemoryManagement::indexAllocation = numCurrentAllocations;
        MemoryManagement::failingAllocation = i + numCurrentAllocations;
        if (MemoryManagement::eventsAllocated[i].event == MemoryManagement::AllocationEvent::EVENT_NEW ||
            MemoryManagement::eventsAllocated[i].event == MemoryManagement::AllocationEvent::EVENT_NEW_ARRAY) {

            continue;
        }

        if (maxIndex != 0 && i > maxIndex) {
            break;
        }

        // Call the method under test
        method(i);

        // Restore allocations
        MemoryManagement::failingAllocation = -1;
    }
    MemoryManagement::failingAllocation = -1;
}

void MemoryManagementFixture::injectFailureOnIndex(InjectedFunction &method, uint32_t index) {
    MemoryManagement::indexAllocation = 0;
    method(-1);
    auto numCurrentAllocations = MemoryManagement::indexAllocation.load();

    // Force a failure
    MemoryManagement::indexAllocation = numCurrentAllocations;
    MemoryManagement::failingAllocation = index + numCurrentAllocations;

    // Call the method under test
    method(index);

    // Restore allocations
    MemoryManagement::failingAllocation = -1;
}
