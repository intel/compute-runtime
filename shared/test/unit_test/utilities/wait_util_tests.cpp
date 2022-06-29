/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
} // namespace CpuIntrinsicsTests

TEST(WaitTest, givenDefaultSettingsWhenNoPollAddressProvidedThenPauseDefaultTimeAndReturnFalse) {
    EXPECT_EQ(1u, WaitUtils::defaultWaitCount);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(nullptr, 0u);
    EXPECT_FALSE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}

TEST(WaitTest, givenDebugFlagOverridesWhenNoPollAddressProvidedThenPauseDefaultTimeAndReturnFalse) {
    DebugManagerStateRestore restore;
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount);

    uint32_t count = 10u;
    DebugManager.flags.WaitLoopCount.set(count);

    WaitUtils::init();
    EXPECT_EQ(count, WaitUtils::waitCount);

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(nullptr, 0u);
    EXPECT_FALSE(ret);
    EXPECT_EQ(oldCount + count, CpuIntrinsicsTests::pauseCounter);
}

TEST(WaitTest, givenDefaultSettingsWhenPollAddressProvidedDoesNotMeetCriteriaThenPauseDefaultTimeAndReturnFalse) {
    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);

    volatile uint32_t pollValue = 1u;
    uint32_t expectedValue = 3;

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue);
    EXPECT_FALSE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}

TEST(WaitTest, givenDefaultSettingsWhenPollAddressProvidedMeetsCriteriaThenPauseDefaultTimeAndReturnTrue) {
    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);

    volatile uint32_t pollValue = 3u;
    uint32_t expectedValue = 1;

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}

TEST(WaitTest, givenDebugFlagSetZeroWhenPollAddressProvidedMeetsCriteriaThenPauseZeroTimesAndReturnTrue) {
    DebugManagerStateRestore restore;
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount);

    uint32_t count = 0u;
    DebugManager.flags.WaitLoopCount.set(count);

    WaitUtils::init();
    EXPECT_EQ(count, WaitUtils::waitCount);

    volatile uint32_t pollValue = 3u;
    uint32_t expectedValue = 1;

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}
