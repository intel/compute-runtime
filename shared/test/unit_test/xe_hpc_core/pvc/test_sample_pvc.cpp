/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PvcOnlyTest = Test<DeviceFixture>;

PVCTEST_F(PvcOnlyTest, WhenGettingHardwareInfoThenPvcIsReturned) {
    EXPECT_EQ(IGFX_PVC, pDevice->getHardwareInfo().platform.eProductFamily);
}
