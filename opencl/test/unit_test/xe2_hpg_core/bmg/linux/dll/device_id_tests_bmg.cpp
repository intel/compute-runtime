/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenBmgSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 13> expectedDescriptors = {{
        {0xE202, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE20B, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE20C, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE20D, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE210, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE211, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE212, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE215, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE216, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE220, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE221, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE222, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
        {0xE223, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
