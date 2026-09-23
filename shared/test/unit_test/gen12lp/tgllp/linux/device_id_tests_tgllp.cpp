/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenTgllpSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 7> expectedDescriptors = {{
        {0x9A49, IGFX_TIGERLAKE_LP},
        {0x9A40, IGFX_TIGERLAKE_LP},
        {0x9A59, IGFX_TIGERLAKE_LP},
        {0x9A60, IGFX_TIGERLAKE_LP},
        {0x9A68, IGFX_TIGERLAKE_LP},
        {0x9A70, IGFX_TIGERLAKE_LP},
        {0x9A78, IGFX_TIGERLAKE_LP},
    }};

    testImpl(expectedDescriptors);
}
