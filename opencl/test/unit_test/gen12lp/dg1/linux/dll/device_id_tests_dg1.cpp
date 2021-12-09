/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"
using namespace NEO;

TEST_F(DeviceIdTests, GivenDg1SupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 4> expectedDescriptors = {{
        {0x4905, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4906, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4907, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4908, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
    }};

    testImpl(expectedDescriptors);
}
