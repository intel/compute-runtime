/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenXeHpSdvSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 1> expectedDescriptors = {{{0x0201, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo}}};

    testImpl(expectedDescriptors);
}
