/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using PvcOnlyTest = Test<ClDeviceFixture>;

PVCTEST_F(PvcOnlyTest, WhenGettingHardwareInfoThenPvcIsReturned) {
    EXPECT_EQ(IGFX_PVC, pDevice->getHardwareInfo().platform.eProductFamily);
}

using XeHpcCoreOnlyTest = Test<ClDeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreOnlyTest, WhenGettingRenderCoreFamilyThenOnlyHpcCoreIsReturned) {
    EXPECT_EQ(IGFX_XE_HPC_CORE, pDevice->getRenderCoreFamily());
}
