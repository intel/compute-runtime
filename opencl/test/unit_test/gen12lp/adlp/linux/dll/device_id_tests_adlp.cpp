/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenAdlpSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 24> expectedDescriptors = {{
        {0x46A0, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46B0, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46A1, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46A2, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46A3, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46A6, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46A8, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46AA, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x462A, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x4626, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x4628, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46B1, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46B2, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46B3, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46C0, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46C1, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46C2, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0x46C3, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0xA7A0, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0xA720, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0xA7A8, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0xA7A1, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0xA721, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
        {0xA7A9, &AdlpHwConfig::hwInfo, &AdlpHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
