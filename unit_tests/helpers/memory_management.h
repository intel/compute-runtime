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

#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace MemoryManagement {
#if defined(__clang__)
#define NO_SANITIZE __attribute__((no_sanitize("undefined")))
#else
#define NO_SANITIZE
#endif

enum LeakDetectionMode {
    STANDARD,
    EXPECT_TO_LEAK,
    TURN_OFF_LEAK_DETECTION
};

struct AllocationEvent {
    enum {
        CallStackSize = 16
    };

    enum EventType {
        EVENT_UNKNOWN,
        EVENT_NEW,
        EVENT_NEW_FAIL,
        EVENT_NEW_NOTHROW,
        EVENT_NEW_NOTHROW_FAIL,
        EVENT_NEW_ARRAY,
        EVENT_NEW_ARRAY_FAIL,
        EVENT_NEW_ARRAY_NOTHROW,
        EVENT_NEW_ARRAY_NOTHROW_FAIL,
        EVENT_DELETE,
        EVENT_DELETE_ARRAY
    };

    EventType event;
    const void *address;
    size_t size;
    int frames;
    void *callstack[CallStackSize];
    int fastLeakDetectionMode = 0;
};
enum : int {
    maxEvents = 1024 * 1024,
    fastEvents = 1024 * 1024
};
extern AllocationEvent eventsAllocated[maxEvents];
extern AllocationEvent eventsDeallocated[maxEvents];

extern void *fastEventsAllocated[maxEvents];
extern void *fastEventsDeallocated[maxEvents];
extern std::atomic<int> fastEventsAllocatedCount;
extern std::atomic<int> fastEventsDeallocatedCount;
extern std::atomic<int> fastLeaksDetectionMode;
extern bool memsetNewAllocations;

extern size_t failingAllocation;
extern std::atomic<size_t> numAllocations;
extern std::atomic<size_t> indexAllocation;
extern std::atomic<size_t> indexDeallocation;
extern size_t breakOnAllocationEvent;
extern size_t breakOnDeallocationEvent;
extern bool logTraces;
extern bool detailedAllocationLoggingActive;
extern int fastLeakDetectionMode;
extern void (*deleteCallback)(void *);

int detectLeaks();
} // namespace MemoryManagement
