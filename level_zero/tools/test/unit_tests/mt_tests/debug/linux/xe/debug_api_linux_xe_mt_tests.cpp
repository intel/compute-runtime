/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_session_fixtures_linux_xe.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

using DebugApiLinuxTestXe = Test<DebugApiLinuxXeFixture>;

TEST_F(DebugApiLinuxTestXe, GivenEventInInternalEventQueueWhenAsyncThreadFunctionIsExecutedThenEventIsHandled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandleToConnection.clear();

    uint8_t eventClientData[sizeof(NEO::EuDebugEventClient)];
    auto client = reinterpret_cast<NEO::EuDebugEventClient *>(&eventClientData);
    client->base.type = static_cast<uint16_t>(NEO::EuDebugParam::eventTypeOpen);
    client->base.flags = static_cast<uint16_t>(NEO::shiftLeftBy(static_cast<uint16_t>(NEO::EuDebugParam::eventBitCreate)));
    client->base.len = sizeof(NEO::EuDebugEventClient);
    client->clientHandle = 0x123456789;

    auto memory = std::make_unique<uint64_t[]>(sizeof(NEO::EuDebugEventClient) / sizeof(uint64_t));
    memcpy(memory.get(), client, sizeof(NEO::EuDebugEventClient));

    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }
    session->internalEventQueue.push(std::move(memory));

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0) {
        ;
    }
    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
    EXPECT_EQ(session->clientHandleToConnection.size(), 1ul);
    EXPECT_NE(session->clientHandleToConnection.find(client->clientHandle), session->clientHandleToConnection.end());

    uint64_t wrongClientHandle = 34;
    EXPECT_EQ(session->clientHandleToConnection.find(wrongClientHandle), session->clientHandleToConnection.end());
}

TEST_F(DebugApiLinuxTestXe, GivenNoEventInInternalEventQueueWhenAsyncThreadFunctionIsExecutedThenEventsAreCheckedForAvailability) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    while (!session->internalEventQueue.empty()) {
        session->internalEventQueue.pop();
    }

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0) {
        ;
    }
    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
}

TEST_F(DebugApiLinuxTestXe, GivenInterruptedThreadsWhenNoAttentionEventIsReadThenThreadUnavailableEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxXe>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxXe::mockClientHandle;

    auto handler = new MockIoctlHandlerXe;
    session->ioctlHandler.reset(handler);
    session->returnTimeDiff = session->interruptTimeout * 10;
    session->synchronousInternalEventRead = true;

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};

    auto result = session->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    session->startAsyncThread();

    while (session->getInternalEventCounter < 2) {
        ;
    }

    session->closeAsyncThread();

    EXPECT_EQ(session->apiEvents.size(), 1u);
    if (session->apiEvents.size() > 0) {
        auto event = session->apiEvents.front();
        EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, event.type);
        EXPECT_EQ(0u, event.info.thread.thread.slice);
        EXPECT_EQ(0u, event.info.thread.thread.subslice);
        EXPECT_EQ(0u, event.info.thread.thread.eu);
        EXPECT_EQ(UINT32_MAX, event.info.thread.thread.thread);
    }
}

} // namespace ult
} // namespace L0
