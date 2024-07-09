/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenLnlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 3> expectedDescriptors = {{
        {0x6420, &LnlHwConfig::hwInfo, &LnlHwConfig::setupHardwareInfo},
        {0x64A0, &LnlHwConfig::hwInfo, &LnlHwConfig::setupHardwareInfo},
        {0x64B0, &LnlHwConfig::hwInfo, &LnlHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
