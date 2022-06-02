/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"
using namespace NEO;

TEST_F(DeviceIdTests, GivenDg1SupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 4> expectedDescriptors = {{
        {0x4905, &Dg1HwConfig::hwInfo, &Dg1HwConfig::setupHardwareInfo},
        {0x4906, &Dg1HwConfig::hwInfo, &Dg1HwConfig::setupHardwareInfo},
        {0x4907, &Dg1HwConfig::hwInfo, &Dg1HwConfig::setupHardwareInfo},
        {0x4908, &Dg1HwConfig::hwInfo, &Dg1HwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
