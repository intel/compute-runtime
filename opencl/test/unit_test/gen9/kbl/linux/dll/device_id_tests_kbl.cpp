/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenKblSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 26> expectedDescriptors = {{
        {0x5902, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x590B, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x590A, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x5906, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x590E, &KBL_1x3x6::hwInfo, &KBL_1x3x6::setupHardwareInfo, GTTYPE_GT1},
        {0x5908, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {0x5913, &KBL_1x3x6::hwInfo, &KBL_1x3x6::setupHardwareInfo, GTTYPE_GT1_5},
        {0x5915, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1_5},

        {0x5912, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591B, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x5917, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591A, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x5916, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591E, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591D, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591C, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x5921, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {0x5926, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x5927, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x592B, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x592A, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x5923, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},

        {0x5932, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {0x593B, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {0x593A, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {0x593D, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
    }};

    testImpl(expectedDescriptors);
}
