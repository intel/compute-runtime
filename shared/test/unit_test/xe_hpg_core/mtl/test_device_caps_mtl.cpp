/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using MtlUsDeviceIdTest = Test<DeviceFixture>;

MTLTEST_F(MtlUsDeviceIdTest, givenMtlProductWhenCheckFp64SupportThenReturnFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}
