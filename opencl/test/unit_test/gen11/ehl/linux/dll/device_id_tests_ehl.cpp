/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenEhlSpportedDeviceIdWhenCheckingHwSetupThenItIsCorrect) {
    std::array<DeviceDescriptor, 9> expectedDescriptors = {{
        {0x4500, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4541, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4551, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4571, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4555, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4E51, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4E61, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4E71, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
        {0x4E55, &EHL_HW_CONFIG::hwInfo, &EHL_HW_CONFIG::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
