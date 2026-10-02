/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <limits>

using namespace NEO;

TEST(TaskCountHelperTest, givenPartitionTagsWithPaddingWhenCheckingReadinessThenOnlyRequestedPartitionsAreChecked) {
    constexpr TaskCountType taskCount = 7;
    constexpr size_t tagOffset = 2 * sizeof(TagAddressType);
    TagAddressType tags[] = {taskCount, 0, taskCount - 1};

    EXPECT_TRUE(TaskCountHelper::isReady(tags, taskCount, 1, tagOffset));
    EXPECT_FALSE(TaskCountHelper::isReady(tags, taskCount, 2, tagOffset));

    tags[2] = taskCount;
    EXPECT_TRUE(TaskCountHelper::isReady(tags, taskCount, 2, tagOffset));
    EXPECT_FALSE(TaskCountHelper::isReady(tags, taskCount + 1, 2, tagOffset));
    EXPECT_TRUE(TaskCountHelper::isReady(tags, taskCount - 1, 2, tagOffset));
}

TEST(TaskCountHelperTest, givenMissingTagsWhenCheckingReadinessThenReturnFalse) {
    EXPECT_FALSE(TaskCountHelper::isReady(nullptr, 0, 1, sizeof(TagAddressType)));
    EXPECT_FALSE(TaskCountHelper::isReady(nullptr, 0, 0, sizeof(TagAddressType)));
}

TEST(TaskCountHelperTest, givenNoPartitionsOrBoundaryTaskCountsWhenCheckingReadinessThenOnlyNumericCompletionIsChecked) {
    TagAddressType tag = 0;
    constexpr auto maximumTaskCount = std::numeric_limits<TaskCountType>::max();

    EXPECT_TRUE(TaskCountHelper::isReady(&tag, maximumTaskCount, 0, 0));
    EXPECT_TRUE(TaskCountHelper::isReady(&tag, 0, 1, 0));
    EXPECT_FALSE(TaskCountHelper::isReady(&tag, maximumTaskCount, 1, 0));

    tag = maximumTaskCount;
    EXPECT_TRUE(TaskCountHelper::isReady(&tag, maximumTaskCount, 1, 0));
}

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern std::atomic<uint32_t> yieldCounter;
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

TEST_F(WaitPredicateOnlyTest, givenNotReadyPollAddressWhenWaitFunctionCalledThenYieldTheCore) {
    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

    volatile TagAddressType pollValue = 1u;
    TaskCountType expectedValue = 3;

    uint32_t oldCount = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_FALSE(WaitUtils::waitFunction(&pollValue, expectedValue, 0));
    EXPECT_EQ(oldCount + 1, CpuIntrinsicsTests::yieldCounter);
}

TEST_F(WaitPredicateOnlyTest, givenNotReadyPollAddressWhenPollFunctionCalledWithoutBlockOnMissThenPollWithoutYieldingTheCore) {
    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

    volatile TagAddressType pollValue = 1u;
    TaskCountType expectedValue = 3;

    uint32_t oldPauseCount = CpuIntrinsicsTests::pauseCounter.load();
    uint32_t oldYieldCount = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_FALSE(WaitUtils::pollFunction(&pollValue, expectedValue, 0, false));
    EXPECT_EQ(oldPauseCount + WaitUtils::waitCount, CpuIntrinsicsTests::pauseCounter);
    EXPECT_EQ(oldYieldCount, CpuIntrinsicsTests::yieldCounter);
}

TEST_F(WaitPredicateOnlyTest, givenReadyPollAddressWhenPollFunctionCalledWithoutBlockOnMissThenReturnTrueWithoutYieldingTheCore) {
    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

    volatile TagAddressType pollValue = 3u;
    TaskCountType expectedValue = 1;

    uint32_t oldCount = CpuIntrinsicsTests::yieldCounter.load();
    EXPECT_TRUE(WaitUtils::pollFunction(&pollValue, expectedValue, 0, false));
    EXPECT_EQ(oldCount, CpuIntrinsicsTests::yieldCounter);
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
