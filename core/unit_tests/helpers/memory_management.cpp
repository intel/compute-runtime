/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/memory_management.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <new>

#if defined(__linux__)
#include <cstdio>
#include <dlfcn.h>
#include <execinfo.h>
#elif defined(_WIN32)
#include <Windows.h>

#include <DbgHelp.h>
#endif

namespace MemoryManagement {
size_t failingAllocation = -1;
std::atomic<size_t> numAllocations(0);
std::atomic<size_t> indexAllocation(0);
std::atomic<size_t> indexDeallocation(0);
bool logTraces = false;
bool fastLeakDetectionEnabled = false;

AllocationEvent eventsAllocated[maxEvents];
AllocationEvent eventsDeallocated[maxEvents];

void *fastEventsAllocated[maxEvents];
void *fastEventsDeallocated[maxEvents];
std::atomic<int> fastEventsAllocatedCount(0);
std::atomic<int> fastEventsDeallocatedCount(0);
std::atomic<int> fastLeaksDetectionMode(LeakDetectionMode::STANDARD);

size_t breakOnAllocationEvent = -1;
size_t breakOnDeallocationEvent = -1;

bool detailedAllocationLoggingActive = false;

// limit size of single allocation in ULT
const size_t maxAllowedAllocationSize = 128 * 1024 * 1024 + 4096;

static void onAllocationEvent() {
    /*
    //switch to true to turn on dillignet breakpoint setting place
    bool setBreakPointHereForLeaks = false;
    if (setBreakPointHereForLeaks) {
        if (breakOnAllocationEvent == indexAllocation.load()) {
            //set breakpoint on line below
            setBreakPointHereForLeaks = false;
        }
    }*/
}

static void onDeallocationEvent(void *) {
    /*
    //switch to true to turn on dillignet breakpoint setting place
    bool setBreakPointHereForLeaks = false;
    if (setBreakPointHereForLeaks) {
        if (breakOnDeallocationEvent == indexDeallocation.load()) {
            //set breakpoint on line below
            setBreakPointHereForLeaks = false;
        }
    }*/
}

void (*deleteCallback)(void *) = onDeallocationEvent;

template <AllocationEvent::EventType typeValid, AllocationEvent::EventType typeFail>
static void *allocate(size_t size) {
    onAllocationEvent();

    if (size > maxAllowedAllocationSize) {
        return nullptr;
    }

    if (!fastLeakDetectionEnabled) {
        return malloc(size);
    }

    void *p;

    if (detailedAllocationLoggingActive) {

        auto indexAllocation = MemoryManagement::indexAllocation.fetch_add(1);
        indexAllocation %= maxEvents;

        auto &eventAllocation = eventsAllocated[indexAllocation];
        eventAllocation.size = size;

        while ((p = malloc(size)) == nullptr) {
            eventAllocation.address = p;
            eventAllocation.event = typeFail;
            throw std::bad_alloc();
        }

        eventAllocation.address = p;
        eventAllocation.event = typeValid;
#if defined(__linux__)
        eventAllocation.frames = logTraces ? backtrace(eventAllocation.callstack, AllocationEvent::CallStackSize) : 0;
#elif defined(_WIN32)
        eventAllocation.frames = logTraces ? CaptureStackBackTrace(0, AllocationEvent::CallStackSize, eventAllocation.callstack, NULL) : 0;
#else
        eventAllocation.frames = 0;
#endif
        eventAllocation.fastLeakDetectionEnabled = fastLeakDetectionEnabled;

        numAllocations++;
    } else {
        p = malloc(size);
    }

    if (fastLeakDetectionEnabled && p && fastLeaksDetectionMode == LeakDetectionMode::STANDARD) {
        auto currentIndex = fastEventsAllocatedCount++;
        fastEventsAllocated[currentIndex] = p;
        assert(currentIndex <= fastEvents);
    }

    return p;
}

template <AllocationEvent::EventType typeValid, AllocationEvent::EventType typeFail>
static void *allocate(size_t size, const std::nothrow_t &) {
    onAllocationEvent();

    if (size > maxAllowedAllocationSize) {
        return nullptr;
    }

    if (!fastLeakDetectionEnabled) {
        return malloc(size);
    }

    void *p;

    if (detailedAllocationLoggingActive) {

        auto indexAllocation = MemoryManagement::indexAllocation.fetch_add(1);
        indexAllocation %= maxEvents;

        p = indexAllocation == failingAllocation
                ? nullptr
                : malloc(size);

        auto &eventAllocation = eventsAllocated[indexAllocation];
        eventAllocation.event = p
                                    ? typeValid
                                    : typeFail;
        eventAllocation.address = p;
        eventAllocation.size = size;
#if defined(__linux__)
        eventAllocation.frames = logTraces ? backtrace(eventAllocation.callstack, AllocationEvent::CallStackSize) : 0;
#elif defined(_WIN32)
        eventAllocation.frames = logTraces ? CaptureStackBackTrace(0, AllocationEvent::CallStackSize, eventAllocation.callstack, NULL) : 0;
#else
        eventAllocation.frames = 0;
#endif
        eventAllocation.fastLeakDetectionEnabled = fastLeakDetectionEnabled;
        numAllocations += p ? 1 : 0;
    } else {
        p = malloc(size);
    }

    if (fastLeakDetectionEnabled && p && fastLeaksDetectionMode == LeakDetectionMode::STANDARD) {
        auto currentIndex = fastEventsAllocatedCount++;
        fastEventsAllocated[currentIndex] = p;
        assert(currentIndex <= fastEvents);
    }

    return p;
}

template <AllocationEvent::EventType typeValid>
static void deallocate(void *p) {
    deleteCallback(p);

    if (!fastLeakDetectionEnabled) {
        if (p) {
            free(p);
        }
        return;
    }

    if (p) {
        if (detailedAllocationLoggingActive) {

            auto indexDeallocation = MemoryManagement::indexDeallocation.fetch_add(1);
            indexDeallocation %= maxEvents;

            --numAllocations;

            auto &eventDeallocation = eventsDeallocated[indexDeallocation];
            eventDeallocation.event = typeValid;
            eventDeallocation.address = p;
            eventDeallocation.size = -1;
#if defined(__linux__)
            eventDeallocation.frames = logTraces ? backtrace(eventDeallocation.callstack, AllocationEvent::CallStackSize) : 0;
#elif defined(_WIN32)
            eventDeallocation.frames = logTraces ? CaptureStackBackTrace(0, AllocationEvent::CallStackSize, eventDeallocation.callstack, NULL) : 0;
#else
            eventDeallocation.frames = 0;
#endif
            eventDeallocation.fastLeakDetectionEnabled = fastLeakDetectionEnabled;
        }
        free(p);

        if (fastLeakDetectionEnabled && p && fastLeaksDetectionMode == LeakDetectionMode::STANDARD) {
            auto currentIndex = fastEventsDeallocatedCount++;
            fastEventsDeallocated[currentIndex] = p;
            assert(currentIndex <= fastEvents);
        }
    }
}
int detectLeaks() {
    int indexLeak = -1;

    for (int allocationIndex = 0u; allocationIndex < fastEventsAllocatedCount; allocationIndex++) {
        auto &eventAllocation = fastEventsAllocated[allocationIndex];
        int deallocationIndex = 0u;
        for (; deallocationIndex < fastEventsDeallocatedCount; deallocationIndex++) {
            if (fastEventsDeallocated[deallocationIndex] == nullptr) {
                continue;
            }
            if (fastEventsDeallocated[deallocationIndex] == eventAllocation) {
                fastEventsDeallocated[deallocationIndex] = nullptr;
                break;
            }
        }
        if (deallocationIndex == fastEventsDeallocatedCount) {
            indexLeak = allocationIndex;
            break;
        }
    }
    return indexLeak;
}

size_t enumerateLeak(size_t indexAllocationTop, size_t indexDeallocationTop, bool lookFromBack, bool requireCallStack) {
    using MemoryManagement::AllocationEvent;
    using MemoryManagement::eventsAllocated;
    using MemoryManagement::eventsDeallocated;

    static auto start = MemoryManagement::invalidLeakIndex;
    auto newIndex = start == MemoryManagement::invalidLeakIndex ? 0 : start;
    bool potentialLeak = false;
    auto potentialLeakIndex = newIndex;

    for (; newIndex < indexAllocationTop; ++newIndex) {
        auto currentIndex = lookFromBack ? indexAllocationTop - newIndex - 1 : newIndex;
        auto &eventAllocation = eventsAllocated[currentIndex];

        if (requireCallStack && eventAllocation.frames == 0) {
            continue;
        }

        if (eventAllocation.event != AllocationEvent::EVENT_UNKNOWN) {
            // Should be some sort of allocation
            size_t deleteIndex = 0;
            for (; deleteIndex < indexDeallocationTop; ++deleteIndex) {
                auto &eventDeallocation = eventsDeallocated[deleteIndex];

                if (eventDeallocation.address == eventAllocation.address &&
                    eventDeallocation.event != AllocationEvent::EVENT_UNKNOWN) {

                    //this memory was once freed, now it is allocated but not freed
                    if (requireCallStack && eventDeallocation.frames == 0) {
                        potentialLeak = true;
                        potentialLeakIndex = currentIndex;
                        continue;
                    }

                    // Clear the NEW and DELETE event.
                    eventAllocation.event = AllocationEvent::EVENT_UNKNOWN;
                    eventDeallocation.event = AllocationEvent::EVENT_UNKNOWN;
                    potentialLeak = false;
                    // Found a corresponding match
                    break;
                }
            }

            if (potentialLeak) {
                return potentialLeakIndex;
            }

            if (deleteIndex == indexDeallocationTop) {
                start = newIndex + 1;
                return currentIndex;
            }
        }
    }
    start = MemoryManagement::invalidLeakIndex;
    return start;
}

std::string printCallStack(const MemoryManagement::AllocationEvent &event) {
    std::string result = "";

    printf("printCallStack.%d\n", event.frames);
    if (!MemoryManagement::captureCallStacks) {
        printf("for detailed stack information turn on captureCallStacks in memory_management.h\n");
    }
    if (event.frames > 0) {
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

} // namespace MemoryManagement

using MemoryManagement::allocate;
using MemoryManagement::AllocationEvent;
using MemoryManagement::deallocate;

#if defined(_WIN32)
#pragma warning(disable : 4290)
#endif
void *operator new(size_t size) {
    return allocate<AllocationEvent::EVENT_NEW, AllocationEvent::EVENT_NEW_FAIL>(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept {
    return allocate<AllocationEvent::EVENT_NEW_NOTHROW, AllocationEvent::EVENT_NEW_NOTHROW_FAIL>(size, std::nothrow);
}

void *operator new[](size_t size) {
    return allocate<AllocationEvent::EVENT_NEW_ARRAY, AllocationEvent::EVENT_NEW_ARRAY_FAIL>(size);
}

void *operator new[](size_t size, const std::nothrow_t &t) noexcept {
    return allocate<AllocationEvent::EVENT_NEW_ARRAY_NOTHROW, AllocationEvent::EVENT_NEW_ARRAY_NOTHROW_FAIL>(size, std::nothrow);
}

void operator delete(void *p) noexcept {
    deallocate<AllocationEvent::EVENT_DELETE>(p);
}

void operator delete[](void *p) noexcept {
    deallocate<AllocationEvent::EVENT_DELETE_ARRAY>(p);
}
void operator delete(void *p, size_t size) noexcept {
    deallocate<AllocationEvent::EVENT_DELETE>(p);
}

void operator delete[](void *p, size_t size) noexcept {
    deallocate<AllocationEvent::EVENT_DELETE_ARRAY>(p);
}
