/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/experimental/source/graph/graph_captured_apis.h"

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

TEST(GraphInternalEventsMtTest, givenEntriesMutexHeldWhenGettingInternalEventByHandleThenLookupWaitsForMutexRelease) {
    Mock<Event> internalEvent;
    WhiteBoxGraphInternalEvents internalEvents;
    const auto hOriginalEvent = reinterpret_cast<ze_event_handle_t>(0x1234u);
    constexpr uint32_t yieldsBeforeInsertion = 1000u;
    std::atomic_bool lookupStarted = false;
    std::atomic_bool lookupFinished = false;
    ze_event_handle_t lookupResult = nullptr;

    std::unique_lock entriesLock(internalEvents.entriesMutex);
    std::thread lookup([&]() {
        lookupStarted = true;
        lookupResult = internalEvents.getInternal(hOriginalEvent);
        lookupFinished = true;
    });
    while (!lookupStarted) {
        std::this_thread::yield();
    }
    for (uint32_t i = 0; (i < yieldsBeforeInsertion) && !lookupFinished; i++) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(lookupFinished);

    internalEvents.entries[hOriginalEvent].internalEvent = &internalEvent;
    entriesLock.unlock();
    lookup.join();

    EXPECT_EQ(internalEvent.toHandle(), lookupResult);
}

} // namespace ult
} // namespace L0
