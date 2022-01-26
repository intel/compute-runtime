/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenPvcSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 2> expectedDescriptors = {{
        {0x0BD0, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BD5, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
