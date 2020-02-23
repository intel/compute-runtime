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

TEST(IcllpDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {IICL_LP_GT1_MOB_DEVICE_F0_ID, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo, GTTYPE_GT1},
        {IICL_LP_1x4x8_LOW_MEDIA_ULT_DEVICE_F0_ID, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo, GTTYPE_GT1},
        {IICL_LP_1x4x8_LOW_MEDIA_ULX_DEVICE_F0_ID, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo, GTTYPE_GT1},

        {IICL_LP_1x6x8_ULX_DEVICE_F0_ID, &ICLLP_1x6x8::hwInfo, &ICLLP_1x6x8::setupHardwareInfo, GTTYPE_GT1},
        {IICL_LP_1x6x8_ULT_DEVICE_F0_ID, &ICLLP_1x6x8::hwInfo, &ICLLP_1x6x8::setupHardwareInfo, GTTYPE_GT1},

        {IICL_LP_1x8x8_SUPERSKU_DEVICE_F0_ID, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo, GTTYPE_GT2},
        {IICL_LP_1x8x8_ULT_DEVICE_F0_ID, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo, GTTYPE_GT2},
        {IICL_LP_1x8x8_ULX_DEVICE_F0_ID, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo, GTTYPE_GT2},
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
