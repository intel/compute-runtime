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

#include "unit_tests/helpers/memory_management.h"
#include "gtest/gtest.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <exception>
#include <iostream>
#include <new>

#if defined(__linux__)
#include <cstdio>
#include <execinfo.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

#if defined(_MSC_VER)
#define NOEXCEPT
#else
#define NOEXCEPT noexcept
#endif

namespace MemoryManagement {
size_t failingAllocation = -1;
std::atomic<size_t> numAllocations(0);
std::atomic<size_t> indexAllocation(0);
std::atomic<size_t> indexDeallocation(0);
bool logTraces = false;
int fastLeakDetectionMode = 0;
bool memsetNewAllocations = false;

AllocationEvent eventsAllocated[maxEvents];
AllocationEvent eventsDeallocated[maxEvents];

void *fastEventsAllocated[maxEvents];
void *fastEventsDeallocated[maxEvents];
std::atomic<int> fastEventsAllocatedCount(0);
std::atomic<int> fastEventsDeallocatedCount(0);
std::atomic<int> fastLeaksDetectionMode(LeakDetectionMode::STANDARD);

size_t breakOnAllocationEvent = -1;
size_t breakOnDeallocationEvent = -1;

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
        std::cerr << "WARNING: Tried to allocate " << size << " bytes but " << maxAllowedAllocationSize << " is alowed!" << std::endl;
        return nullptr;
    }

    if (!fastLeakDetectionMode) {
        return malloc(size);
    }

    auto indexAllocation = MemoryManagement::indexAllocation.fetch_add(1);
    indexAllocation %= maxEvents;

    auto &eventAllocation = eventsAllocated[indexAllocation];
    eventAllocation.size = size;

    void *p;
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
    eventAllocation.fastLeakDetectionMode = fastLeakDetectionMode;

    numAllocations++;

    if (fastLeakDetectionMode && p && fastLeaksDetectionMode == LeakDetectionMode::STANDARD) {
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
        std::cerr << "WARNING: Tried to allocate " << size << " bytes but " << maxAllowedAllocationSize << " is alowed!" << std::endl;
        return nullptr;
    }

    if (!fastLeakDetectionMode) {
        return malloc(size);
    }

    auto indexAllocation = MemoryManagement::indexAllocation.fetch_add(1);
    indexAllocation %= maxEvents;

    auto p = indexAllocation == failingAllocation
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
    eventAllocation.fastLeakDetectionMode = fastLeakDetectionMode;
    numAllocations += p ? 1 : 0;

    if (fastLeakDetectionMode && p && fastLeaksDetectionMode == LeakDetectionMode::STANDARD) {
        auto currentIndex = fastEventsAllocatedCount++;
        fastEventsAllocated[currentIndex] = p;
        assert(currentIndex <= fastEvents);
    }

    return p;
}

template <AllocationEvent::EventType typeValid>
static void deallocate(void *p) {
    deleteCallback(p);

    if (!fastLeakDetectionMode) {
        if (p) {
            free(p);
        }
        return;
    }

    if (p) {
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
        eventDeallocation.fastLeakDetectionMode = fastLeakDetectionMode;
        free(p);

        if (fastLeakDetectionMode && p && fastLeaksDetectionMode == LeakDetectionMode::STANDARD) {
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
} // namespace MemoryManagement

using MemoryManagement::allocate;
using MemoryManagement::AllocationEvent;
using MemoryManagement::deallocate;

NO_SANITIZE
inline void initMemory(void *p, size_t size) {
    if ((p == nullptr) || (false == MemoryManagement::memsetNewAllocations)) {
        return;
    }

    memset(p, 0, size);
}

#if defined(_WIN32)
#pragma warning(disable : 4290)
#endif
void *operator new(size_t size) {
    void *p = allocate<AllocationEvent::EVENT_NEW, AllocationEvent::EVENT_NEW_FAIL>(size);
    initMemory(p, size);
    return p;
}

void *operator new(size_t size, const std::nothrow_t &) NOEXCEPT {
    void *p = allocate<AllocationEvent::EVENT_NEW_NOTHROW, AllocationEvent::EVENT_NEW_NOTHROW_FAIL>(size, std::nothrow);
    initMemory(p, size);
    return p;
}

void *operator new[](size_t size) {
    void *p = allocate<AllocationEvent::EVENT_NEW_ARRAY, AllocationEvent::EVENT_NEW_ARRAY_FAIL>(size);
    initMemory(p, size);
    return p;
}

void *operator new[](size_t size, const std::nothrow_t &t) NOEXCEPT {
    void *p = allocate<AllocationEvent::EVENT_NEW_ARRAY_NOTHROW, AllocationEvent::EVENT_NEW_ARRAY_NOTHROW_FAIL>(size, std::nothrow);
    initMemory(p, size);
    return p;
}

void operator delete(void *p) throw() {
    deallocate<AllocationEvent::EVENT_DELETE>(p);
}

void operator delete[](void *p) throw() {
    deallocate<AllocationEvent::EVENT_DELETE_ARRAY>(p);
}
