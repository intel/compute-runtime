/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenCriSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 5> expectedDescriptors = {{
        {0x674C, IGFX_CRI},
        {0x674D, IGFX_CRI},
        {0x674E, IGFX_CRI},
        {0x674F, IGFX_CRI},
        {0x6750, IGFX_CRI},
    }};

    testImpl(expectedDescriptors);
}
