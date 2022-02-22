/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

namespace NEO {

using MockOfflineCompilerPvcTests = ::testing::Test;

PVCTEST_F(MockOfflineCompilerPvcTests, GivenPvcXlA0ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto pvcConfig = PVC_XL_A0;
    auto pvcXlId = PVC_XL_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(pvcConfig);
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_PVC);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x0);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, pvcXlId);
}

PVCTEST_F(MockOfflineCompilerPvcTests, GivenPvcXlB0ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto pvcConfig = PVC_XL_B0;
    auto pvcXlId = PVC_XL_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(pvcConfig);
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_PVC);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x01);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, pvcXlId);
}

PVCTEST_F(MockOfflineCompilerPvcTests, GivenPvcXtA0ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto pvcConfig = PVC_XT_A0;
    auto pvcXtId = PVC_XT_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(pvcConfig);
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_PVC);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x03);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, pvcXtId);
}

PVCTEST_F(MockOfflineCompilerPvcTests, GivenPvcXtB0ConfigWhenInitHardwareInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto pvcConfig = PVC_XT_B0;
    auto pvcXtId = PVC_XT_IDS.front();
    mockOfflineCompiler.deviceName = mockOfflineCompiler.argHelper->parseProductConfigFromValue(pvcConfig);
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, IGFX_PVC);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, 0x1E);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, pvcXtId);
}

} // namespace NEO