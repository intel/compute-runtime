/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenBmgSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 14> expectedDescriptors = {{
        {0xE202, IGFX_BMG},
        {0xE209, IGFX_BMG},
        {0xE20B, IGFX_BMG},
        {0xE20C, IGFX_BMG},
        {0xE20D, IGFX_BMG},
        {0xE210, IGFX_BMG},
        {0xE211, IGFX_BMG},
        {0xE212, IGFX_BMG},
        {0xE215, IGFX_BMG},
        {0xE216, IGFX_BMG},
        {0xE220, IGFX_BMG},
        {0xE221, IGFX_BMG},
        {0xE222, IGFX_BMG},
        {0xE223, IGFX_BMG},
    }};

    testImpl(expectedDescriptors);
}
