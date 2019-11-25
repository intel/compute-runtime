/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

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

struct AllocationEvent { // NOLINT(clang-analyzer-optin.performance.Padding)
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
    bool fastLeakDetectionEnabled = false;
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
extern bool fastLeakDetectionEnabled;
extern void (*deleteCallback)(void *);

constexpr auto nonfailingAllocation = static_cast<size_t>(-1);
constexpr auto invalidLeakIndex = static_cast<size_t>(-1);

// capture allocations call stacks to print them during memory leak in ULTs
constexpr bool captureCallStacks = false;

int detectLeaks();
std::string printCallStack(const MemoryManagement::AllocationEvent &event);
size_t enumerateLeak(size_t indexAllocationTop, size_t indexDeallocationTop, bool lookFromEnd, bool requireCallStack);

} // namespace MemoryManagement
