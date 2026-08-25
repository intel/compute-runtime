/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/affinity_mask.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(IsAffinityMaskSetTests, GivenDefaultValueWhenCheckingIfAffinityMaskIsSetThenFalseIsReturned) {
    EXPECT_FALSE(isAffinityMaskSet("default"));
}

TEST(IsAffinityMaskSetTests, GivenEmptyValueWhenCheckingIfAffinityMaskIsSetThenFalseIsReturned) {
    EXPECT_FALSE(isAffinityMaskSet(""));
}

TEST(IsAffinityMaskSetTests, GivenExplicitValueWhenCheckingIfAffinityMaskIsSetThenTrueIsReturned) {
    EXPECT_TRUE(isAffinityMaskSet("0.1"));
}

} // namespace NEO
