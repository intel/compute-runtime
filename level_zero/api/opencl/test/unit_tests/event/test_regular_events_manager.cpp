/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/event/regular_events_manager.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include <level_zero/ze_api.h>

namespace NEO {
namespace LEO {
namespace ult {

struct WhiteBoxRegularEventsManager : public RegularEventsManager {
    using RegularEventsManager::RegularEventsManager;
    size_t hostVisibleFreeCount() const { return this->hostVisibleGroup.freeEvents.size(); }
    size_t timestampFreeCount() const { return this->timestampGroup.freeEvents.size(); }
    size_t hostVisiblePoolCount() const { return this->hostVisibleGroup.pools.size(); }
};

using RegularEventsManagerTests = Test<OclFixture>;

TEST_F(RegularEventsManagerTests, givenCompletedEventWhenReturnedThenItIsPooledAndReusedOnNextObtain) {
    WhiteBoxRegularEventsManager manager(context->toHandle(), {device->toHandle()});

    auto firstEvent = manager.obtainEvent(false);
    ASSERT_NE(nullptr, firstEvent);
    EXPECT_EQ(0u, manager.hostVisibleFreeCount());

    zeEventHostSignal(firstEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(firstEvent));

    manager.returnEvent(firstEvent, false);
    EXPECT_EQ(1u, manager.hostVisibleFreeCount());

    auto secondEvent = manager.obtainEvent(false);
    EXPECT_EQ(firstEvent, secondEvent);
    EXPECT_EQ(0u, manager.hostVisibleFreeCount());
    EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(secondEvent));

    manager.returnEvent(secondEvent, false);
}

TEST_F(RegularEventsManagerTests, givenIncompleteEventWhenReturnedThenItIsNotPooled) {
    WhiteBoxRegularEventsManager manager(context->toHandle(), {device->toHandle()});

    auto event = manager.obtainEvent(false);
    ASSERT_NE(nullptr, event);

    zeEventHostReset(event);
    ASSERT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(event));

    manager.returnEvent(event, false);
    EXPECT_EQ(0u, manager.hostVisibleFreeCount());
}

TEST_F(RegularEventsManagerTests, givenTimestampAndHostVisibleRequestsThenSeparatePoolsAreUsed) {
    WhiteBoxRegularEventsManager manager(context->toHandle(), {device->toHandle()});

    auto hostVisibleEvent = manager.obtainEvent(false);
    auto timestampEvent = manager.obtainEvent(true);
    ASSERT_NE(nullptr, hostVisibleEvent);
    ASSERT_NE(nullptr, timestampEvent);
    EXPECT_NE(hostVisibleEvent, timestampEvent);

    EXPECT_FALSE(L0::Event::fromHandle(hostVisibleEvent)->isEventTimestampFlagSet());
    EXPECT_TRUE(L0::Event::fromHandle(timestampEvent)->isEventTimestampFlagSet());

    zeEventHostSignal(hostVisibleEvent);
    zeEventHostSignal(timestampEvent);
    manager.returnEvent(hostVisibleEvent, false);
    manager.returnEvent(timestampEvent, true);
    EXPECT_EQ(1u, manager.hostVisibleFreeCount());
    EXPECT_EQ(1u, manager.timestampFreeCount());
}

TEST_F(RegularEventsManagerTests, givenTimestampEventReturnedCompletedThenReusedFromTimestampPool) {
    WhiteBoxRegularEventsManager manager(context->toHandle(), {device->toHandle()});

    auto firstEvent = manager.obtainEvent(true);
    zeEventHostSignal(firstEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(firstEvent));

    manager.returnEvent(firstEvent, true);
    EXPECT_EQ(1u, manager.timestampFreeCount());

    auto secondEvent = manager.obtainEvent(true);
    EXPECT_EQ(firstEvent, secondEvent);
    EXPECT_EQ(0u, manager.timestampFreeCount());

    manager.returnEvent(secondEvent, true);
}

TEST_F(RegularEventsManagerTests, givenEventPoolCreationFailsWhenObtainingEventThenNullptrReturnedAndNoPoolIsStored) {
    WhiteBoxRegularEventsManager manager(context->toHandle(), {nullptr});

    auto event = manager.obtainEvent(false);
    EXPECT_EQ(nullptr, event);
    EXPECT_EQ(0u, manager.hostVisibleFreeCount());
    EXPECT_EQ(0u, manager.hostVisiblePoolCount());

    EXPECT_EQ(nullptr, manager.obtainEvent(false));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
