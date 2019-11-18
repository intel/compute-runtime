/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"

#include <array>

using namespace NEO;

TEST(TglLpDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {IGEN12LP_GT1_MOB_DEVICE_F0_ID, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x6x16_ULT_15W_DEVICE_F0_ID, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x6x16_ULX_5_2W_DEVICE_F0_ID, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x6x16_ULT_12W_DEVICE_F0_ID, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x2x16_HALO_45W_DEVICE_F0_ID, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x2x16_DESK_65W_DEVICE_F0_ID, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x2x16_HALO_WS_45W_DEVICE_F0_ID, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo, GTTYPE_GT2},
        {ITGL_LP_1x2x16_DESK_WS_65W_DEVICE_F0_ID, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo, GTTYPE_GT2},
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
