/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

using EventPoolCreate = Test<DeviceFixture>;
using EventCreate = Test<DeviceFixture>;

TEST_F(EventPoolCreate, allocationContainsAtLeast16Bytes) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(device, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minAllocationSize = eventPool->getEventSize();
    EXPECT_GE(allocation->getUnderlyingBufferSize(), minAllocationSize);
}

TEST_F(EventPoolCreate, givenTimestampEventsThenVerifyNumTimestampsToRead) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_TIMESTAMP, // all events in pool are visible to Host
        1};

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(device, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    uint32_t numTimestamps = 4u;
    EXPECT_EQ(numTimestamps, eventPool->getNumEventTimestampsToRead());
}

TEST_F(EventPoolCreate, givenAnEventIsCreatedFromThisEventPoolThenEventContainsDeviceCommandStreamReceiver) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};
    const ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_HOST,
        ZE_EVENT_SCOPE_FLAG_HOST};

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(device, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);
}

TEST_F(EventCreate, givenAnEventCreatedThenTheEventHasTheDeviceCommandStreamReceiverSet) {
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_EVENT_POOL_DESC_VERSION_CURRENT,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};
    const ze_event_desc_t eventDesc = {
        ZE_EVENT_DESC_VERSION_CURRENT,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(device, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    std::unique_ptr<L0::Event> event(Event::create(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event);
    ASSERT_NE(nullptr, event.get()->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event.get()->csr);
}

class TimestampEventCreate : public Test<DeviceFixture> {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        ze_event_pool_desc_t eventPoolDesc = {
            ZE_EVENT_POOL_DESC_VERSION_CURRENT,
            ZE_EVENT_POOL_FLAG_TIMESTAMP,
            1};

        ze_event_desc_t eventDesc = {
            ZE_EVENT_DESC_VERSION_CURRENT,
            0,
            ZE_EVENT_SCOPE_FLAG_NONE,
            ZE_EVENT_SCOPE_FLAG_NONE};

        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(device, &eventPoolDesc));
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::Event>(L0::Event::create(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, eventPool);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::Event> event;
};

TEST_F(TimestampEventCreate, givenEventCreatedWithTimestampThenIsTimestampEventFlagSet) {
    EXPECT_TRUE(event->isTimestampEvent);
}

TEST_F(TimestampEventCreate, givenEventTimestampsNotTriggeredThenValuesInInitialState) {
    uint64_t globalStart, globalEnd, contextStart, contextEnd;

    event->getTimestamp(ZE_EVENT_TIMESTAMP_GLOBAL_START, &globalStart);
    event->getTimestamp(ZE_EVENT_TIMESTAMP_GLOBAL_END, &globalEnd);
    event->getTimestamp(ZE_EVENT_TIMESTAMP_CONTEXT_START, &contextStart);
    event->getTimestamp(ZE_EVENT_TIMESTAMP_CONTEXT_END, &contextEnd);

    EXPECT_EQ(static_cast<uint64_t>(Event::STATE_CLEARED), globalStart);
    EXPECT_EQ(static_cast<uint64_t>(Event::STATE_CLEARED), globalEnd);
    EXPECT_EQ(static_cast<uint64_t>(Event::STATE_CLEARED), contextStart);
    EXPECT_EQ(static_cast<uint64_t>(Event::STATE_CLEARED), contextEnd);
}

TEST_F(TimestampEventCreate, givenSingleTimestampEventThenAllocationSizeCreatedForAllTimestamps) {
    auto allocation = &eventPool->getAllocation();
    ASSERT_NE(nullptr, allocation);

    uint32_t minTimestampEventAllocation = eventPool->getEventSize() *
                                           eventPool->getNumEventTimestampsToRead();
    EXPECT_GE(minTimestampEventAllocation, allocation->getUnderlyingBufferSize());
}

} // namespace ult
} // namespace L0