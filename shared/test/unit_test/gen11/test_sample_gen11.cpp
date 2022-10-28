/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen11OnlyTest = ::testing::Test;

GEN11TEST_F(Gen11OnlyTest, WhenGettingRenderCoreFamilyThenGen11CoreIsReturned) {
    EXPECT_EQ(IGFX_GEN11_CORE, defaultHwInfo->platform.eRenderCoreFamily);
}
