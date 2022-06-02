/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenGlkSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 2> expectedDescriptors = {{
        {0x3184, &GlkHw1x3x6::hwInfo, &GlkHw1x3x6::setupHardwareInfo},
        {0x3185, &GlkHw1x2x6::hwInfo, &GlkHw1x2x6::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
