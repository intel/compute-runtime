/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/event_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

using TimestampEvent = Test<EventFixture<1, 1>>;
using TimestampUsedPacketSignalEvent = Test<EventUsedPacketSignalFixture<1, 1, 0, -1>>;

GEN12LPTEST_F(TimestampEvent, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet) {
    typename MockTimestampPackets32::Packet data = {};
    data.contextStart = 1u;
    data.contextEnd = 2u;
    data.globalStart = 3u;
    data.globalEnd = 4u;

    event->hostAddressFromPool = &data;
    ze_kernel_timestamp_result_t result = {};

    event->queryKernelTimestamp(&result);
    EXPECT_EQ(data.globalStart, result.context.kernelStart);
    EXPECT_EQ(data.globalEnd, result.context.kernelEnd);
    EXPECT_EQ(data.globalStart, result.global.kernelStart);
    EXPECT_EQ(data.globalEnd, result.global.kernelEnd);
}

GEN12LPTEST_F(TimestampUsedPacketSignalEvent, givenEventMoreThanOneTimestampsPacketWhenQueryKernelTimestampThenCorrectCalculationAreMade) {
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

    event->hostAddressFromPool = &data;
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
