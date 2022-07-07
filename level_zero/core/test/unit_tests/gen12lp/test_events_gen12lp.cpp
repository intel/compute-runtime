/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {
struct TimestampEvent : public Test<DeviceFixture> {
    void SetUp() override {
        DeviceFixture::SetUp();
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;
        eventDesc.wait = 0;

        ze_result_t result = ZE_RESULT_SUCCESS;
        eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ASSERT_NE(nullptr, eventPool);
        event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event);
    }

    void TearDown() override {
        event.reset(nullptr);
        eventPool.reset(nullptr);
        DeviceFixture::TearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool;
    std::unique_ptr<L0::Event> event;
};

GEN12LPTEST_F(TimestampEvent, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet) {
    typename MockTimestampPackets32::Packet data = {};
    data.contextStart = 1u;
    data.contextEnd = 2u;
    data.globalStart = 3u;
    data.globalEnd = 4u;

    event->hostAddress = &data;
    ze_kernel_timestamp_result_t result = {};

    event->queryKernelTimestamp(&result);
    EXPECT_EQ(data.globalStart, result.context.kernelStart);
    EXPECT_EQ(data.globalEnd, result.context.kernelEnd);
    EXPECT_EQ(data.globalStart, result.global.kernelStart);
    EXPECT_EQ(data.globalEnd, result.global.kernelEnd);
}

GEN12LPTEST_F(TimestampEvent, givenEventMoreThanOneTimestampsPacketWhenQueryKernelTimestampThenCorrectCalculationAreMade) {
    typename MockTimestampPackets32::Packet data[3] = {};
    data[0].contextStart = 3u;
    data[0].contextEnd = 4u;
    data[0].globalStart = 5u;
    data[0].globalEnd = 6u;
    data[1].contextStart = 2u;
    data[1].contextEnd = 6u;
    data[1].globalStart = 4u;
    data[1].globalEnd = 8u;
    data[2].contextStart = 4u;
    data[2].contextEnd = 5u;
    data[2].globalStart = 6u;
    data[2].globalEnd = 7u;

    event->hostAddress = &data;
    event->setPacketsInUse(3u);
    ze_kernel_timestamp_result_t result = {};

    event->queryKernelTimestamp(&result);
    EXPECT_EQ(data[1].globalStart, result.context.kernelStart);
    EXPECT_EQ(data[1].globalEnd, result.context.kernelEnd);
    EXPECT_EQ(data[1].globalStart, result.global.kernelStart);
    EXPECT_EQ(data[1].globalEnd, result.global.kernelEnd);
}
} // namespace ult
} // namespace L0
