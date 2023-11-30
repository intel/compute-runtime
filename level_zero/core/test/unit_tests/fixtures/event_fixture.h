/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

struct EventFixtureImpl : public DeviceFixture {
    void setUp() {
        setUpImpl(0, 0);
    }

    void setUpImpl(int32_t eventPoolHostFlag, int32_t eventPoolTimestampFlag);

    void tearDown();

    DebugManagerStateRestore restorer;

    std::unique_ptr<L0::EventPool> eventPool = nullptr;
    std::unique_ptr<EventImp<uint32_t>> event;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
};

template <int32_t eventPoolHostFlag, int32_t eventPoolTimestampFlag>
struct EventFixture : public EventFixtureImpl {
    void setUp() {
        EventFixtureImpl::setUpImpl(eventPoolHostFlag, eventPoolTimestampFlag);
    }

    void tearDown() {
        EventFixtureImpl::tearDown();
    }
};

template <int32_t eventPoolHostFlag, int32_t eventPoolTimestampFlag, int32_t signalAllEventPackets, int32_t compactEventPackets>
struct EventUsedPacketSignalFixture : public EventFixture<eventPoolHostFlag, eventPoolTimestampFlag> {
    void setUp() {
        NEO::debugManager.flags.SignalAllEventPackets.set(signalAllEventPackets);

        NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.set(compactEventPackets);
        NEO::debugManager.flags.CompactL3FlushEventPacket.set(compactEventPackets);

        EventFixture<eventPoolHostFlag, eventPoolTimestampFlag>::setUp();
    }
};

} // namespace ult
} // namespace L0
