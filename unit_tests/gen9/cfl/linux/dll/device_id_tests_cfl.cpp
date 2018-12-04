/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"

#include <array>

using namespace OCLRT;

TEST(CflDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 23> expectedDescriptors = {{
        {ICFL_GT1_DT_DEVICE_F0_ID, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {ICFL_GT1_S41_DT_DEVICE_F0_ID, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {ICFL_GT1_S61_DT_DEVICE_F0_ID, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {ICFL_GT1_41F_2F1F_ULT_DEVICE_F0_ID, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {ICFL_GT1_S6_S4_S2_F1F_DT_DEVICE_F0_ID, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {ICFL_GT1_U41F_U2F1F_ULT_DEVICE_F0_ID, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {ICFL_GT2_DT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_HALO_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_HALO_WS_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_S42_DT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_S62_DT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_SERV_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_S82_S6F2_DT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_U42F_U2F1F_ULT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_U42F_U2F2F_ULT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_U42F_U2F2_ULT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {ICFL_GT2_S8_S2_DT_DEVICE_F0_ID, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {ICFL_HALO_DEVICE_F0_ID, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {ICFL_GT3_ULT_15W_DEVICE_F0_ID, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {ICFL_GT3_ULT_15W_42EU_DEVICE_F0_ID, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {ICFL_GT3_ULT_28W_DEVICE_F0_ID, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {ICFL_GT3_ULT_DEVICE_F0_ID, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {ICFL_GT3_U43_ULT_DEVICE_F0_ID, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
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
