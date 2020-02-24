/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"

#include "test.h"

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
HWCMDTEST_F(IGFX_GEN8_CORE, ExcludeTestBdw, givenHwCmdTestWhenBdwExcludedDontRunOnBdw) {
    EXPECT_NE(IGFX_BROADWELL, ::productFamily);
}
HWTEST_F(ExcludeTestBdw, givenHwTestWhenBdwExcludedDontRunOnBdw) {
    EXPECT_NE(IGFX_BROADWELL, ::productFamily);
}

using ExcludeTestSkl = ExcludeTest<IGFX_SKYLAKE>;
HWCMDTEST_F(IGFX_GEN8_CORE, ExcludeTestSkl, givenHwCmdTestWhenSklExcludedDontRunOnSkl) {
    EXPECT_NE(IGFX_SKYLAKE, ::productFamily);
}
HWTEST_F(ExcludeTestSkl, givenHwTestWhenSklExcludedDontRunOnSkl) {
    EXPECT_NE(IGFX_SKYLAKE, ::productFamily);
}
