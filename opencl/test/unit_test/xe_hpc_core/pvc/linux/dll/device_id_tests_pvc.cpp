/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenPvcSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0x0BD0, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BD5, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BD6, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BD7, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BD8, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BD9, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BDA, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
        {0x0BDB, &PVC_CONFIG::hwInfo, &PVC_CONFIG::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
