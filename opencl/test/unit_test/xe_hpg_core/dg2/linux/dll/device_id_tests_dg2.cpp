/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenDg2SupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 20> expectedDescriptors = {{
        {0x4F80, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x4F81, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x4F82, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x4F83, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x4F84, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x4F87, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x4F88, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x5690, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x5691, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x5692, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x5693, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x5694, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x5695, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56A0, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56A1, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56A2, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56A5, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56A6, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56C0, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
        {0x56C1, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
