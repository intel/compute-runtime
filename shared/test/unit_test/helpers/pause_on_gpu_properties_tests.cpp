/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <iterator>

using namespace NEO;

TEST(PauseOnGpuPropertiesTest, givenDisabledFlagWhenSelectingPausesThenNothingIsSelected) {
    const auto pauses = PauseOnGpuProperties::selectPauses(PauseOnGpuProperties::DebugFlagValues::Disabled, 0);
    EXPECT_FALSE(pauses.beforeWorkload);
    EXPECT_FALSE(pauses.afterWorkload);

    const auto pauseSpace = PauseOnGpuProperties::selectPauseSpace(PauseOnGpuProperties::DebugFlagValues::Disabled, 0, true);
    EXPECT_FALSE(pauseSpace.beforeWorkload);
    EXPECT_FALSE(pauseSpace.afterWorkload);
}

TEST(PauseOnGpuPropertiesTest, givenNumberedFlagWhenSelectingPausesThenOnlyMatchingTaskCountIsSelected) {
    const auto matching = PauseOnGpuProperties::selectPauses(3, 3);
    EXPECT_TRUE(matching.beforeWorkload);
    EXPECT_TRUE(matching.afterWorkload);

    const auto notMatching = PauseOnGpuProperties::selectPauses(3, 2);
    EXPECT_FALSE(notMatching.beforeWorkload);
    EXPECT_FALSE(notMatching.afterWorkload);
}

TEST(PauseOnGpuPropertiesTest, givenPauseModeWhenSelectingPausesThenOnlyRequestedModeIsSelected) {
    DebugManagerStateRestore restorer;

    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);
    auto pauses = PauseOnGpuProperties::selectPauses(PauseOnGpuProperties::DebugFlagValues::OnEachEnqueue, 7);
    EXPECT_TRUE(pauses.beforeWorkload);
    EXPECT_FALSE(pauses.afterWorkload);

    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);
    pauses = PauseOnGpuProperties::selectPauses(PauseOnGpuProperties::DebugFlagValues::OnEachEnqueue, 7);
    EXPECT_FALSE(pauses.beforeWorkload);
    EXPECT_TRUE(pauses.afterWorkload);
}

TEST(PauseOnGpuPropertiesTest, givenSelectionAtExecutionWhenSelectingPauseSpaceThenTaskCountIsIgnored) {
    const auto pauseSpace = PauseOnGpuProperties::selectPauseSpace(3, 0, true);
    EXPECT_TRUE(pauseSpace.beforeWorkload);
    EXPECT_TRUE(pauseSpace.afterWorkload);
}

TEST(PauseOnGpuPropertiesTest, givenSelectionAtSubmissionWhenSelectingPauseSpaceThenTaskCountMustMatch) {
    const auto notMatching = PauseOnGpuProperties::selectPauseSpace(3, 0, false);
    EXPECT_FALSE(notMatching.beforeWorkload);
    EXPECT_FALSE(notMatching.afterWorkload);

    const auto matching = PauseOnGpuProperties::selectPauseSpace(3, 3, false);
    EXPECT_TRUE(matching.beforeWorkload);
    EXPECT_TRUE(matching.afterWorkload);
}

TEST(PauseOnGpuPropertiesTest, givenNumberedFlagWhenClaimingSubmissionPausesThenOnlyFirstStartAndLastEndAreKept) {
    PauseOnGpuProperties::PendingSubmissionPauses pendingPauses{};
    uint8_t firstEnd[4] = {1, 1, 1, 1};
    uint8_t secondEnd[4] = {2, 2, 2, 2};
    uint8_t unused = 0;

    EXPECT_TRUE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, 0, true, &unused, sizeof(unused)));
    EXPECT_FALSE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, 0, true, &unused, sizeof(unused)));

    EXPECT_TRUE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, 0, false, firstEnd, sizeof(firstEnd)));
    EXPECT_EQ(firstEnd, pendingPauses.afterWorkloadCommand);

    EXPECT_TRUE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, 0, false, secondEnd, sizeof(secondEnd)));
    EXPECT_EQ(secondEnd, pendingPauses.afterWorkloadCommand);
    EXPECT_TRUE(std::all_of(std::begin(firstEnd), std::end(firstEnd), [](uint8_t value) { return value == 0; }));
    EXPECT_TRUE(std::all_of(std::begin(secondEnd), std::end(secondEnd), [](uint8_t value) { return value == 2; }));
}

TEST(PauseOnGpuPropertiesTest, givenOnEachEnqueueFlagWhenClaimingSubmissionPausesThenEveryPauseIsKeptWithoutTracking) {
    PauseOnGpuProperties::PendingSubmissionPauses pendingPauses{};
    uint8_t end[4] = {1, 1, 1, 1};

    EXPECT_TRUE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, PauseOnGpuProperties::DebugFlagValues::OnEachEnqueue, true, end, sizeof(end)));
    EXPECT_TRUE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, PauseOnGpuProperties::DebugFlagValues::OnEachEnqueue, true, end, sizeof(end)));
    EXPECT_TRUE(PauseOnGpuProperties::claimSubmissionPause(pendingPauses, PauseOnGpuProperties::DebugFlagValues::OnEachEnqueue, false, end, sizeof(end)));

    EXPECT_FALSE(pendingPauses.beforeWorkloadProgrammed);
    EXPECT_EQ(nullptr, pendingPauses.afterWorkloadCommand);
}
