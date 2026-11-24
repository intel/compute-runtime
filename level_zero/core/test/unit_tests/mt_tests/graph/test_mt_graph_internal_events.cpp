/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/experimental/source/graph/graph_captured_apis.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace L0 {
namespace ult {

struct WhiteBoxGraphInternalEvents : public GraphInternalEvents {
    using GraphInternalEvents::entries;
    using GraphInternalEvents::entriesMutex;
};

template <typename LookupT>
void lookupWhileEntriesMutexHeldThenInsertEntry(WhiteBoxGraphInternalEvents &internalEvents, ze_event_handle_t hOriginalEvent, L0::Event *internalEvent, LookupT lookup) {
    constexpr uint32_t yieldsBeforeInsertion = 1000u;
    std::atomic_bool lookupStarted = false;
    std::atomic_bool lookupFinished = false;

    std::unique_lock entriesLock(internalEvents.entriesMutex);
    std::thread lookupThread([&]() {
        lookupStarted = true;
        lookup();
        lookupFinished = true;
    });
    while (!lookupStarted) {
        std::this_thread::yield();
    }
    for (uint32_t i = 0; (i < yieldsBeforeInsertion) && !lookupFinished; i++) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(lookupFinished);

    internalEvents.entries[hOriginalEvent].internalEvent = internalEvent;
    entriesLock.unlock();
    lookupThread.join();
}

TEST(GraphInternalEventsMtTest, givenEntriesMutexHeldWhenGettingInternalEventByHandleThenLookupWaitsForMutexRelease) {
    Mock<Event> internalEvent;
    WhiteBoxGraphInternalEvents internalEvents;
    const auto hOriginalEvent = reinterpret_cast<ze_event_handle_t>(0x1234u);
    ze_event_handle_t lookupResult = nullptr;

    lookupWhileEntriesMutexHeldThenInsertEntry(internalEvents, hOriginalEvent, &internalEvent, [&]() {
        lookupResult = internalEvents.getInternal(hOriginalEvent);
    });

    EXPECT_EQ(internalEvent.toHandle(), lookupResult);
}

TEST(GraphInternalEventsMtTest, givenEntriesMutexHeldWhenGettingInternalEventByEventThenLookupWaitsForMutexRelease) {
    Mock<Event> originalEvent;
    Mock<Event> internalEvent;
    WhiteBoxGraphInternalEvents internalEvents;
    L0::Event *lookupResult = nullptr;

    lookupWhileEntriesMutexHeldThenInsertEntry(internalEvents, originalEvent.toHandle(), &internalEvent, [&]() {
        lookupResult = internalEvents.getInternal(&originalEvent);
    });

    EXPECT_EQ(&internalEvent, lookupResult);
}

} // namespace ult
} // namespace L0
