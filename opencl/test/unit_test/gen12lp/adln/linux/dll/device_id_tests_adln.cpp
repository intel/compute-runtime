/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adln.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/test_macros/test.h"

#include <array>

using namespace NEO;

TEST(AdlnDeviceIdTest, GivenSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 3> expectedDescriptors = {{
        {0x46D0, &AdlnHwConfig::hwInfo, &AdlnHwConfig::setupHardwareInfo},
        {0x46D1, &AdlnHwConfig::hwInfo, &AdlnHwConfig::setupHardwareInfo},
        {0x46D2, &AdlnHwConfig::hwInfo, &AdlnHwConfig::setupHardwareInfo},
    }};

    auto compareStructs = [](const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->pHwInfo == second->pHwInfo &&
               first->setupHardwareInfo == second->setupHardwareInfo;
    };

    size_t startIndex = 0;
    while (!compareStructs(&expectedDescriptors[0], &deviceDescriptorTable[startIndex]) &&
           deviceDescriptorTable[startIndex].deviceId != 0) {
        startIndex++;
    };
    EXPECT_NE(0u, deviceDescriptorTable[startIndex].deviceId);

    for (auto &expected : expectedDescriptors) {
        EXPECT_TRUE(compareStructs(&expected, &deviceDescriptorTable[startIndex]));
        startIndex++;
    }
}
