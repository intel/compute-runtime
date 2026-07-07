/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

namespace testing {
class TestInfo;
}

namespace NEO {
namespace TestMatcherRegistry {

using MatchFunc = bool (*)(PRODUCT_FAMILY);

void registerMatcher(const char *testName, MatchFunc matchFunc);
bool willRunForCurrentProduct(const char *testName, const PRODUCT_FAMILY productFamily);

// Strips gtest's runtime instantiation/index decorations, then checks both the registered matcher and NEO::TestExcludes.
bool willRunForCurrentProduct(const ::testing::TestInfo &testInfo, const PRODUCT_FAMILY productFamily);

} // namespace TestMatcherRegistry
} // namespace NEO
