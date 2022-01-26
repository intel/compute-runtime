/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenAdlsSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 7> expectedDescriptors = {{
        {0x4680, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
        {0x4682, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
        {0x4688, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
        {0x468A, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
        {0x4690, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
        {0x4692, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
        {0x4693, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
