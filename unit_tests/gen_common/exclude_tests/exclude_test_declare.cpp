/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "hw_cmds.h"

#include <type_traits>

using ExcludeTest = ::testing::Test;

HWCMDTEST_F(IGFX_GEN8_CORE, ExcludeTest, whenBdwExcludedDontRunOnBdw) {
    EXPECT_NE(IGFX_BROADWELL, ::productFamily);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ExcludeTest, whenSklExcludedDontRunOnSkl) {
    EXPECT_NE(IGFX_SKYLAKE, ::productFamily);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ExcludeTest, whenCnlExcludedDontRunOnCnl) {
    EXPECT_NE(IGFX_CANNONLAKE, ::productFamily);
}
