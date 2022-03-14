/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using XeHpcCoreOnlyTest = Test<DeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreOnlyTest, WhenGettingRenderCoreFamilyThenOnlyHpcCoreIsReturned) {
    EXPECT_EQ(IGFX_XE_HPC_CORE, pDevice->getRenderCoreFamily());
}
