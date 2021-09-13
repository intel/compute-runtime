/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"

#include "test.h"

using namespace NEO;

TEST(ImplicitScalingApiTests, givenOpenClApiUsedThenSupportEnabled) {
    EXPECT_TRUE(ImplicitScaling::apiSupport);
}

TEST(ImplicitScalingApiTests, givenOpenClApiUsedThenSemaphoreProgrammingRequiredIsFalse) {
    EXPECT_FALSE(ImplicitScaling::semaphoreProgrammingRequired);
}

TEST(ImplicitScalingApiTests, givenOpenClApiUsedThenCrossTileAtomicSynchronization) {
    EXPECT_TRUE(ImplicitScaling::crossTileAtomicSynchronization);
}
