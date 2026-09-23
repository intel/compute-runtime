/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, givenMtlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 4> expectedDescriptors = {{{0x7D40, IGFX_METEORLAKE},
                                                            {0x7D55, IGFX_METEORLAKE},
                                                            {0x7DD5, IGFX_METEORLAKE},
                                                            {0x7D45, IGFX_METEORLAKE}}};

    testImpl(expectedDescriptors);
}
