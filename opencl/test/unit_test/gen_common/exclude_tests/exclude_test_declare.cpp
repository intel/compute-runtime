/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <type_traits>

template <uint32_t prohibitedValue>
struct ExcludeTest : ::testing::Test {
    void SetUp() override {
        EXPECT_NE(prohibitedValue, ::productFamily);
    }
    void TearDown() override {
        EXPECT_NE(prohibitedValue, ::productFamily);
    }
};

using ExcludeTestBdw = ExcludeTest<IGFX_BROADWELL>;
HWCMDTEST_F(IGFX_GEN12LP_CORE, ExcludeTestBdw, givenHwCmdTestWhenBdwExcludedThenDontRunOnBdw) {
    EXPECT_NE(IGFX_BROADWELL, ::productFamily);
}
HWTEST_F(ExcludeTestBdw, givenHwTestWhenBdwExcludedThenDontRunOnBdw) {
    EXPECT_NE(IGFX_BROADWELL, ::productFamily);
}

using ExcludeTestSkl = ExcludeTest<IGFX_SKYLAKE>;
HWCMDTEST_F(IGFX_GEN12LP_CORE, ExcludeTestSkl, givenHwCmdTestWhenSklExcludedThenDontRunOnSkl) {
    EXPECT_NE(IGFX_SKYLAKE, ::productFamily);
}
HWTEST_F(ExcludeTestSkl, givenHwTestWhenSklExcludedThenDontRunOnSkl) {
    EXPECT_NE(IGFX_SKYLAKE, ::productFamily);
}
