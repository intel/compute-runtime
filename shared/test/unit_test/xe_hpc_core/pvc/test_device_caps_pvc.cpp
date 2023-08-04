/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using PvcDeviceCapsTests = Test<DeviceFixture>;

PVCTEST_F(PvcDeviceCapsTests, givenPvcProductWhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

PVCTEST_F(PvcDeviceCapsTests, givenPvcWhenAskingForCacheFlushAfterWalkerThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

PVCTEST_F(PvcDeviceCapsTests, givenPvcProductWhenCheckImagesSupportThenReturnFalse) {
    EXPECT_FALSE(PVC::hwInfo.capabilityTable.supportsImages);
}
