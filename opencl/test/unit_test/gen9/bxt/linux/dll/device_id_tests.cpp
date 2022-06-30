/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenBxtSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0x9906, &BxtHw1x3x6::hwInfo, &BxtHw1x3x6::setupHardwareInfo},
        {0x9907, &BxtHw1x3x6::hwInfo, &BxtHw1x3x6::setupHardwareInfo},
        {0x0A84, &BxtHw1x3x6::hwInfo, &BxtHw1x3x6::setupHardwareInfo},
        {0x5A84, &BxtHw1x3x6::hwInfo, &BxtHw1x3x6::setupHardwareInfo},
        {0x5A85, &BxtHw1x2x6::hwInfo, &BxtHw1x2x6::setupHardwareInfo},
        {0x1A85, &BxtHw1x2x6::hwInfo, &BxtHw1x2x6::setupHardwareInfo},
        {0x1A84, &BxtHw1x3x6::hwInfo, &BxtHw1x3x6::setupHardwareInfo},
        {0x9908, &BxtHw1x3x6::hwInfo, &BxtHw1x3x6::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
