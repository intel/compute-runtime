/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "hw_cmds.h"

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

using ExcludeTestCnl = ExcludeTest<IGFX_CANNONLAKE>;
HWCMDTEST_F(IGFX_GEN8_CORE, ExcludeTestCnl, givenHwCmdTestWhenCnlExcludedDontRunOnCnl) {
    EXPECT_NE(IGFX_CANNONLAKE, ::productFamily);
}
HWTEST_F(ExcludeTestCnl, givenHwTestWhenCnlExcludedDontRunOnCnl) {
    EXPECT_NE(IGFX_CANNONLAKE, ::productFamily);
}
