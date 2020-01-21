/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_neo.h"
#include "test.h"

#include <array>

using namespace NEO;

TEST(EhlDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 7> expectedDescriptors = {{
        {IEHL_1x4x8_SUPERSKU_DEVICE_A0_ID, &EHL_1x4x8::hwInfo, &EHL_1x4x8::setupHardwareInfo, GTTYPE_GT1},
        {IEHL_1x2x4_DEVICE_A0_ID, &EHL_1x2x4::hwInfo, &EHL_1x2x4::setupHardwareInfo, GTTYPE_GT1},
        {IEHL_1x4x4_DEVICE_A0_ID, &EHL_1x4x4::hwInfo, &EHL_1x4x4::setupHardwareInfo, GTTYPE_GT1},
        {IEHL_1x4x8_DEVICE_A0_ID, &EHL_1x4x8::hwInfo, &EHL_1x4x8::setupHardwareInfo, GTTYPE_GT1},
        {IJSL_1x4x4_DEVICE_B0_ID, &EHL_1x4x4::hwInfo, &EHL_1x4x4::setupHardwareInfo, GTTYPE_GT1},
        {IJSL_1x4x6_DEVICE_B0_ID, &EHL_1x4x6::hwInfo, &EHL_1x4x6::setupHardwareInfo, GTTYPE_GT1},
        {IJSL_1x4x8_DEVICE_B0_ID, &EHL_1x4x8::hwInfo, &EHL_1x4x8::setupHardwareInfo, GTTYPE_GT1},
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
