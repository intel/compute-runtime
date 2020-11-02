/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "test.h"

#include <array>

using namespace NEO;

TEST(IcllpDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0xFF05, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo, GTTYPE_GT1},
        {0x8A56, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo, GTTYPE_GT1},
        {0x8A58, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo, GTTYPE_GT1},

        {0x8A5C, &ICLLP_1x6x8::hwInfo, &ICLLP_1x6x8::setupHardwareInfo, GTTYPE_GT1},
        {0x8A5A, &ICLLP_1x6x8::hwInfo, &ICLLP_1x6x8::setupHardwareInfo, GTTYPE_GT1},

        {0x8A50, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo, GTTYPE_GT2},
        {0x8A52, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo, GTTYPE_GT2},
        {0x8A51, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo, GTTYPE_GT2},
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
