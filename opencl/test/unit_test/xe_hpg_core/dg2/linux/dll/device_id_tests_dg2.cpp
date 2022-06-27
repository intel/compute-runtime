/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenDg2SupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 30> expectedDescriptors = {{
        {0x4F80, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F81, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F82, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F83, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F84, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F85, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F86, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F87, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x4F88, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5690, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5691, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5692, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5693, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5694, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5695, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5696, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x5697, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A3, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A4, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56B0, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56B1, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56B2, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56B3, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A0, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A1, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A2, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A5, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56A6, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56C0, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
        {0x56C1, &Dg2HwConfig::hwInfo, &Dg2HwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
