/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
} // namespace CpuIntrinsicsTests

struct WaitPredicateOnlyFixture {
    void setUp() {
        debugManager.flags.EnableWaitpkg.set(0);
        backupWaitCount = std::make_unique<VariableBackup<uint32_t>>(&WaitUtils::waitCount);
    }

    void tearDown() {}

    DebugManagerStateRestore restore;
    std::unique_ptr<VariableBackup<uint32_t>> backupWaitCount;
};

using WaitPredicateOnlyTest = Test<WaitPredicateOnlyFixture>;

TEST_F(WaitPredicateOnlyTest, givenDefaultSettingsWhenNoPollAddressProvidedThenPauseDefaultTimeAndReturnFalse) {
    EXPECT_EQ(1u, WaitUtils::defaultWaitCount);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(nullptr, 0u, 0);
    EXPECT_FALSE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}

TEST_F(WaitPredicateOnlyTest, givenDebugFlagOverridesWhenNoPollAddressProvidedThenPauseDefaultTimeAndReturnFalse) {
    uint32_t count = 10u;
    debugManager.flags.WaitLoopCount.set(count);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(count, WaitUtils::waitCount);

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(nullptr, 0u, 0);
    EXPECT_FALSE(ret);
    EXPECT_EQ(oldCount + count, CpuIntrinsicsTests::pauseCounter);
}

TEST_F(WaitPredicateOnlyTest, givenDefaultSettingsWhenPollAddressProvidedDoesNotMeetCriteriaThenPauseDefaultTimeAndReturnFalse) {
    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);

    volatile TagAddressType pollValue = 1u;
    TaskCountType expectedValue = 3;

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue, 0);
    EXPECT_FALSE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}

TEST_F(WaitPredicateOnlyTest, givenDefaultSettingsWhenPollAddressProvidedMeetsCriteriaThenPauseDefaultTimeAndReturnTrue) {
    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);

    volatile TagAddressType pollValue = 3u;
    TaskCountType expectedValue = 1;

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue, 0);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}

TEST_F(WaitPredicateOnlyTest, givenDebugFlagSetZeroWhenPollAddressProvidedMeetsCriteriaThenPauseZeroTimesAndReturnTrue) {
    uint32_t count = 0u;
    debugManager.flags.WaitLoopCount.set(count);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(count, WaitUtils::waitCount);

    volatile TagAddressType pollValue = 3u;
    TaskCountType expectedValue = 1;

    uint32_t oldCount = CpuIntrinsicsTests::pauseCounter.load();
    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue, 0);
    EXPECT_TRUE(ret);
    EXPECT_EQ(oldCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
}
