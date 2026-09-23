/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenRklSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 6> expectedDescriptors = {{{0x4C80, IGFX_ROCKETLAKE},
                                                            {0x4C8A, IGFX_ROCKETLAKE},
                                                            {0x4C8B, IGFX_ROCKETLAKE},
                                                            {0x4C8C, IGFX_ROCKETLAKE},
                                                            {0x4C90, IGFX_ROCKETLAKE},
                                                            {0x4C9A, IGFX_ROCKETLAKE}}};

    testImpl(expectedDescriptors);
}
