/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/memory_management_fixture.h"

#include "core/unit_tests/helpers/memory_management.h"
#include "runtime/helpers/options.h"
#include "unit_tests/memory_leak_listener.h"

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

bool printMemoryOpCallStack = true;

void MemoryManagementFixture::SetUp() {
    EXPECT_EQ(static_cast<size_t>(-1), MemoryManagement::failingAllocation);
    MemoryManagement::indexAllocation = 0;
    MemoryManagement::indexDeallocation = 0;
    MemoryManagement::failingAllocation = -1;
    previousAllocations = MemoryManagement::numAllocations.load();
    MemoryManagement::logTraces = NEO::captureCallStacks;
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

std::string printCallStack(const MemoryManagement::AllocationEvent &event) {
    std::string result = "";

    printf("printCallStack.%d.%d\n", printMemoryOpCallStack, event.frames);
    if (!NEO::captureCallStacks) {
        printf("for detailed stack information turn on captureCallStacks in memory_management_fixture.h\n");
    }
    if (printMemoryOpCallStack && event.frames > 0) {
#if defined(__linux__)
        char **bt = backtrace_symbols(event.callstack, event.frames);
        char *demangled;
        int status;
        char output[1024];
        Dl_info info;
        result += "\n";
        for (int i = 0; i < event.frames; ++i) {
            dladdr(event.callstack[i], &info);
            if (info.dli_sname) {
                demangled = nullptr;
                status = -1;
                if (info.dli_sname[0] == '_') {
                    demangled = abi::__cxa_demangle(info.dli_sname, nullptr, 0, &status);
                }
                snprintf(output, sizeof(output), "%-3d %*p %s + %zd\n",
                         (event.frames - i - 1), (int)(sizeof(void *) + 2), event.callstack[i],
                         status == 0 ? demangled : info.dli_sname == 0 ? bt[i] : info.dli_sname,
                         (char *)event.callstack[i] - (char *)info.dli_saddr);
                free(demangled);
            } else {
                snprintf(output, sizeof(output), "%-3d %*p %s\n",
                         (event.frames - i - 1), (int)(sizeof(void *) + 2), event.callstack[i], bt[i]);
            }
            result += std::string(output);
        }
        result += "\n";
        free(bt);
#elif defined(_WIN32)
        SYMBOL_INFO *symbol;
        HANDLE process;
        process = GetCurrentProcess();
        SymInitialize(process, NULL, TRUE);

        symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        for (int i = 0; i < event.frames; i++) {
            SymFromAddr(process, (DWORD64)(event.callstack[i]), 0, symbol);

            printf("%i: %s - 0x%0" PRIx64 "\n", event.frames - i - 1, symbol->Name, symbol->Address);
        }

        free(symbol);
#endif
    }

    return result;
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
