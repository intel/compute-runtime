/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenAdlpSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 18> expectedDescriptors = {{
        {0x46A0, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46B0, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46A1, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46A2, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46A3, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46A6, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46A8, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46AA, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x462A, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x4626, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x4628, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46B1, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46B2, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46B3, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46C0, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46C1, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46C2, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
        {0x46C3, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
