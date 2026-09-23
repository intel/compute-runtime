/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenAdlsSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 16> expectedDescriptors = {{
        {0x4680, IGFX_ALDERLAKE_S},
        {0x4682, IGFX_ALDERLAKE_S},
        {0x4688, IGFX_ALDERLAKE_S},
        {0x468A, IGFX_ALDERLAKE_S},
        {0x468B, IGFX_ALDERLAKE_S},
        {0x4690, IGFX_ALDERLAKE_S},
        {0x4692, IGFX_ALDERLAKE_S},
        {0x4693, IGFX_ALDERLAKE_S},
        {0xA780, IGFX_ALDERLAKE_S},
        {0xA781, IGFX_ALDERLAKE_S},
        {0xA782, IGFX_ALDERLAKE_S},
        {0xA783, IGFX_ALDERLAKE_S},
        {0xA788, IGFX_ALDERLAKE_S},
        {0xA789, IGFX_ALDERLAKE_S},
        {0xA78A, IGFX_ALDERLAKE_S},
        {0xA78B, IGFX_ALDERLAKE_S},
    }};

    testImpl(expectedDescriptors);
}
