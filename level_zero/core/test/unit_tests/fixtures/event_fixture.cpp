/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/event_fixture.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/event/event_impl.inl"

#include "gtest/gtest.h"

namespace L0 {
template struct EventImp<uint32_t>;
template struct EventImp<uint64_t>;

namespace ult {

void EventFixtureImpl::setUpImpl(int32_t eventPoolHostFlag, int32_t eventPoolTimestampFlag) {
    NEO::debugManager.flags.EnableWaitpkg.set(0);

    DeviceFixture::setUp();

    ze_event_pool_flags_t eventPoolFlags = 0;
    if (eventPoolHostFlag == 1) {
        eventPoolFlags |= ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    }
    if (eventPoolTimestampFlag == 1) {
        eventPoolFlags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }

    eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 4;
    eventPoolDesc.flags = eventPoolFlags;

    eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;

    ze_result_t result = ZE_RESULT_SUCCESS;
    eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, eventPool);
    event = std::unique_ptr<EventImp<uint32_t>>(static_cast<EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device)));
    ASSERT_NE(nullptr, event);
}

void EventFixtureImpl::tearDown() {
    event.reset(nullptr);
    eventPool.reset(nullptr);
    DeviceFixture::tearDown();
}

} // namespace ult
} // namespace L0
