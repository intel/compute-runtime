/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

namespace NEO {

using MockOfflineCompilerDg2Tests = ::testing::Test;

DG2TEST_F(MockOfflineCompilerDg2Tests, GivenDg2G10A0ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    PRODUCT_CONFIG dg2Config = DG2_G10_A0;
    auto dg2G10Id = DG2_G10_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(dg2Config);

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_DG2);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x0);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, dg2G10Id);
}

DG2TEST_F(MockOfflineCompilerDg2Tests, GivenDg2G10B0ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    PRODUCT_CONFIG dg2Config = DG2_G10_B0;
    auto dg2G10Id = DG2_G10_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(dg2Config);

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_DG2);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x4);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, dg2G10Id);
}

DG2TEST_F(MockOfflineCompilerDg2Tests, GivenDg2G11ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    PRODUCT_CONFIG dg2Config = DG2_G11;
    auto dg2G11Id = DG2_G11_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(dg2Config);

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_DG2);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x0);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, dg2G11Id);
}

} // namespace NEO