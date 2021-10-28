/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> DeviceTest;

TEST_F(DeviceTest, whenBlitterOperationsSupportIsDisabledThenNoInternalCopyEngineIsReturned) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;
    EXPECT_EQ(nullptr, pDevice->getInternalCopyEngine());
}
