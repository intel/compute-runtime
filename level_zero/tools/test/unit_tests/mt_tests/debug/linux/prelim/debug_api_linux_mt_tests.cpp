/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/i915_prelim.h"
// Force prelim headers over upstream headers
// prevent including any other headers to avoid redefinition errors
#define _I915_DRM_H_

#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/prelim/debug_session_fixtures_linux.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include "level_zero/zet_intel_gpu_debug.h"

namespace NEO {
namespace SysCalls {
extern uint32_t closeFuncCalled;
extern int closeFuncArgPassed;
extern int closeFuncRetVal;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

using DebugApiLinuxAsyncThreadTest = Test<DebugApiLinuxPrelimFixture>;

TEST_F(DebugApiLinuxAsyncThreadTest, GivenDebugSessionWhenStartingAndClosingAsyncThreadThenThreadIsStartedAndFinishes) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxi915>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto handler = new MockIoctlHandlerI915;
    session->ioctlHandler.reset(handler);

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

TEST_F(DebugApiLinuxAsyncThreadTest, GivenDebugSessionWhenStartingAndClosingInternalEventsAsyncThreadThenThreadIsStartedAndFinishes) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxi915>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto handler = new MockIoctlHandlerI915;
    session->ioctlHandler.reset(handler);

    session->startInternalEventsThread();

    while (handler->pollCounter == 0) {
        ;
    }

    EXPECT_TRUE(session->internalEventThread.threadActive);

    session->closeInternalEventsThread();

    EXPECT_FALSE(session->internalEventThread.threadActive);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenDebugSessionWithAsyncThreadWhenClosingConnectionThenAsyncThreadIsTerminated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockAsyncThreadDebugSessionLinuxi915>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto handler = new MockIoctlHandlerI915;
    session->ioctlHandler.reset(handler);

    session->startAsyncThread();

    while (session->getInternalEventCounter == 0) {
        ;
    }

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeConnection();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenInterruptedThreadsWhenNoAttentionEventIsReadThenThreadUnavailableEventIsGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxi915>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto handler = new MockIoctlHandlerI915;
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

    if (session->apiEvents.size() > 0) {
        auto event = session->apiEvents.front();
        EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, event.type);
        EXPECT_EQ(0u, event.info.thread.thread.slice);
        EXPECT_EQ(0u, event.info.thread.thread.subslice);
        EXPECT_EQ(0u, event.info.thread.thread.eu);
        EXPECT_EQ(UINT32_MAX, event.info.thread.thread.thread);
    }
}

TEST_F(DebugApiLinuxAsyncThreadTest, GivenInterruptedThreadsWhenNoAttentionEventIsReadWithinTimeoutThenNoEventGenerated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxi915>(config, device, 10);
    ASSERT_NE(nullptr, session);

    session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto handler = new MockIoctlHandlerI915;
    session->ioctlHandler.reset(handler);
    session->returnTimeDiff = 0;

    session->startAsyncThread();

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};
    auto result = session->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    while (session->getInternalEventCounter == 0) {
        ;
    }

    session->closeAsyncThread();

    EXPECT_EQ(0u, session->apiEvents.size());
}

using TileAttachAsyncThreadTest = Test<TileAttachFixture<>>;

TEST_F(TileAttachAsyncThreadTest, GivenInterruptedThreadsWhenNoAttentionEventIsReadThenThreadUnavailableEventIsGenerated) {
    rootSession->tileSessions[0].second = true;
    tileSessions[0]->returnTimeDiff = rootSession->interruptTimeout * 10;

    ze_device_thread_t thread = {0, 0, 0, 0};
    auto result = tileSessions[0]->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    rootSession->synchronousInternalEventRead = true;
    rootSession->startAsyncThread();

    while (rootSession->getInternalEventCounter < 2) {
        ;
    }

    rootSession->closeAsyncThread();

    zet_debug_event_t event = {};
    result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, event.type);
    EXPECT_EQ(0u, event.info.thread.thread.slice);
    EXPECT_EQ(0u, event.info.thread.thread.subslice);
    EXPECT_EQ(0u, event.info.thread.thread.eu);
    EXPECT_EQ(0u, event.info.thread.thread.thread);
}

} // namespace ult
} // namespace L0
