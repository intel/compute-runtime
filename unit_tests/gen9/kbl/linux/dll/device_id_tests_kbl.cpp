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

using namespace NEO;

TEST(KblDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 26> expectedDescriptors = {{
        {IKBL_GT1_DT_DEVICE_F0_ID, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IKBL_GT1_HALO_DEVICE_F0_ID, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IKBL_GT1_SERV_DEVICE_F0_ID, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IKBL_GT1_ULT_DEVICE_F0_ID, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IKBL_GT1_ULX_DEVICE_F0_ID, &KBL_1x3x6::hwInfo, &KBL_1x3x6::setupHardwareInfo, GTTYPE_GT1},
        {IKBL_GT1F_HALO_DEVICE_F0_ID, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {IKBL_GT1_5_ULT_DEVICE_F0_ID, &KBL_1x3x6::hwInfo, &KBL_1x3x6::setupHardwareInfo, GTTYPE_GT1_5},
        {IKBL_GT1_5_ULX_DEVICE_F0_ID, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1_5},

        {IKBL_GT2_DT_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_HALO_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_R_ULT_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_SERV_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_ULT_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_ULX_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_WRK_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2_R_ULX_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IKBL_GT2F_ULT_DEVICE_F0_ID, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {IKBL_GT3_15W_ULT_DEVICE_F0_ID, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IKBL_GT3_28W_ULT_DEVICE_F0_ID, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IKBL_GT3_HALO_DEVICE_F0_ID, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IKBL_GT3_SERV_DEVICE_F0_ID, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IKBL_GT3_ULT_DEVICE_F0_ID, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},

        {IKBL_GT4_DT_DEVICE_F0_ID, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {IKBL_GT4_HALO_DEVICE_F0_ID, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {IKBL_GT4_SERV_DEVICE_F0_ID, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {IKBL_GT4_WRK_DEVICE_F0_ID, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
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
