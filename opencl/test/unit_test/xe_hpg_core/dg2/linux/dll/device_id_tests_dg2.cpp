/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenDg2SupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 2> expectedDescriptors = {{
        {0x4F80, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo, GTTYPE_GT4},
        {0x4F87, &DG2_CONFIG::hwInfo, &DG2_CONFIG::setupHardwareInfo, GTTYPE_GT4},
    }};

    testImpl(expectedDescriptors);
}
