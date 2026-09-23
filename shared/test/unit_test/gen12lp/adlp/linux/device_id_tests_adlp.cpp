/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenAdlpSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 25> expectedDescriptors = {{
        {0x46A0, IGFX_ALDERLAKE_P},
        {0x46B0, IGFX_ALDERLAKE_P},
        {0x46A1, IGFX_ALDERLAKE_P},
        {0x46A3, IGFX_ALDERLAKE_P},
        {0x46A6, IGFX_ALDERLAKE_P},
        {0x46A8, IGFX_ALDERLAKE_P},
        {0x46AA, IGFX_ALDERLAKE_P},
        {0x462A, IGFX_ALDERLAKE_P},
        {0x4626, IGFX_ALDERLAKE_P},
        {0x4628, IGFX_ALDERLAKE_P},
        {0x46B1, IGFX_ALDERLAKE_P},
        {0x46B3, IGFX_ALDERLAKE_P},
        {0x46C0, IGFX_ALDERLAKE_P},
        {0x46C1, IGFX_ALDERLAKE_P},
        {0x46C3, IGFX_ALDERLAKE_P},
        {0xA7A0, IGFX_ALDERLAKE_P},
        {0xA720, IGFX_ALDERLAKE_P},
        {0xA7A8, IGFX_ALDERLAKE_P},
        {0xA7A1, IGFX_ALDERLAKE_P},
        {0xA721, IGFX_ALDERLAKE_P},
        {0xA7A9, IGFX_ALDERLAKE_P},
        {0xA7AA, IGFX_ALDERLAKE_P},
        {0xA7AB, IGFX_ALDERLAKE_P},
        {0xA7AC, IGFX_ALDERLAKE_P},
        {0xA7AD, IGFX_ALDERLAKE_P},
    }};

    testImpl(expectedDescriptors);
}
