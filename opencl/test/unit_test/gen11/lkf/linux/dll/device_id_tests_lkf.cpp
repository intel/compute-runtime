/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenLkfSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 1> expectedDescriptors = {{
        {0x9840, &LkfHw1x8x8::hwInfo, &LkfHw1x8x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
