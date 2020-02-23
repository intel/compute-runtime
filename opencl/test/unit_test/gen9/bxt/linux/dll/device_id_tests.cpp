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

TEST(BxtDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {IBXT_A_DEVICE_F0_ID, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_C_DEVICE_F0_ID, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_GT_3x6_DEVICE_ID, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_P_3x6_DEVICE_ID, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_P_12EU_3x6_DEVICE_ID, &BXT_1x2x6::hwInfo, &BXT_1x2x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_PRO_12EU_3x6_DEVICE_ID, &BXT_1x2x6::hwInfo, &BXT_1x2x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_PRO_3x6_DEVICE_ID, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {IBXT_X_DEVICE_F0_ID, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
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
