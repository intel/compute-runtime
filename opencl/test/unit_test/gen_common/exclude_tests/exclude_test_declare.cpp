/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

template <uint32_t prohibitedValue>
struct ExcludeTest : ::testing::Test {
    void SetUp() override {
        EXPECT_NE(prohibitedValue, ::productFamily);
    }
    void TearDown() override {
        EXPECT_NE(prohibitedValue, ::productFamily);
    }
};

using ExcludeTestPtl = ExcludeTest<IGFX_PTL>;
HWCMDTEST_F(IGFX_GEN12LP_CORE, ExcludeTestPtl, givenHwCmdTestWhenPtlExcludedThenDontRunOnPtl) {
    EXPECT_NE(IGFX_PTL, ::productFamily);
}
HWTEST_F(ExcludeTestPtl, givenHwTestWhenPtlExcludedThenDontRunOnPtl) {
    EXPECT_NE(IGFX_PTL, ::productFamily);
}
