/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/windows/mock_wddm_eudebug.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/tools/test/unit_tests/sources/debug/windows/debug_session_fixtures_windows.h"

namespace L0 {
namespace ult {

using DebugApiWindowsAsyncThreadTest = Test<DebugApiWindowsFixture>;

TEST_F(DebugApiWindowsAsyncThreadTest, GivenDebugSessionWhenStartingAsyncThreadThenThreadIsStarted) {
    auto session = std::make_unique<MockDebugSessionWindows>(zet_debug_config_t{0x1234}, device);
    ASSERT_NE(nullptr, session);
    session->debugHandle = MockDebugSessionWindows::mockDebugHandle;
    session->wddm = mockWddm;
    session->startAsyncThread();

    EXPECT_TRUE(session->asyncThread.threadActive);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
}

TEST_F(DebugApiWindowsAsyncThreadTest, GivenDebugSessionWhenStartingAndClosingAsyncThreadThenThreadIsStartedAndFinishes) {
    auto session = std::make_unique<MockAsyncThreadDebugSessionWindows>(zet_debug_config_t{0x1234}, device);
    ASSERT_NE(nullptr, session);
    session->debugHandle = MockDebugSessionWindows::mockDebugHandle;
    session->wddm = mockWddm;
    session->startAsyncThread();

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeAsyncThread();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
}

TEST_F(DebugApiWindowsAsyncThreadTest, GivenDebugSessionWithAsyncThreadWhenClosingConnectionThenAsyncThreadIsTerminated) {
    auto session = std::make_unique<MockAsyncThreadDebugSessionWindows>(zet_debug_config_t{0x1234}, device);
    ASSERT_NE(nullptr, session);
    session->debugHandle = MockDebugSessionWindows::mockDebugHandle;
    session->wddm = mockWddm;
    session->startAsyncThread();

    EXPECT_TRUE(session->asyncThread.threadActive);
    EXPECT_FALSE(session->asyncThreadFinished);

    session->closeConnection();

    EXPECT_FALSE(session->asyncThread.threadActive);
    EXPECT_TRUE(session->asyncThreadFinished);
}

} // namespace ult
} // namespace L0
