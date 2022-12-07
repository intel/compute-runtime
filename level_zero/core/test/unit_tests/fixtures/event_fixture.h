/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

template <int32_t eventPoolHostFlag, int32_t eventPoolTimestampFlag>
struct EventFixture : public DeviceFixture {
    void setUp() {
        ze_event_pool_flags_t eventPoolFlags = 0;
        if (eventPoolHostFlag == 1) {
            eventPoolFlags |= ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        }
        if (eventPoolTimestampFlag == 1) {
            eventPoolFlags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        }

        DeviceFixture::setUp();
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
        event = std::unique_ptr<L0::EventImp<uint32_t>>(static_cast<L0::EventImp<uint32_t> *>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device)));
        ASSERT_NE(nullptr, event);
    }

    void tearDown() {
        event.reset(nullptr);
        eventPool.reset(nullptr);
        DeviceFixture::tearDown();
    }

    std::unique_ptr<L0::EventPool> eventPool = nullptr;
    std::unique_ptr<L0::EventImp<uint32_t>> event;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
};

template <int32_t eventPoolHostFlag, int32_t eventPoolTimestampFlag, int32_t signalAllEventPackets, int32_t compactEventPackets>
struct EventUsedPacketSignalFixture : public EventFixture<eventPoolHostFlag, eventPoolTimestampFlag> {
    void setUp() {
        NEO::DebugManager.flags.SignalAllEventPackets.set(signalAllEventPackets);

        NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.set(compactEventPackets);
        NEO::DebugManager.flags.CompactL3FlushEventPacket.set(compactEventPackets);

        EventFixture<eventPoolHostFlag, eventPoolTimestampFlag>::setUp();
    }

    DebugManagerStateRestore restorer;
};

} // namespace ult
} // namespace L0
