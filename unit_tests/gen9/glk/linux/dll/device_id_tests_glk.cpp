/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"

#include "hw_cmds.h"

#include <array>

using namespace OCLRT;

TEST(GlkDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 1> expectedDescriptors = {{
        {IGLK_GT2_ULT_18EU_DEVICE_F0_ID, &GLK_1x3x6::hwInfo, &GLK_1x3x6::setupHardwareInfo, GTTYPE_GTA},
    }};

    auto compareStructs = [](const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->pHwInfo == second->pHwInfo &&
               first->setupHardwareInfo == second->setupHardwareInfo && first->eGtType == second->eGtType;
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
