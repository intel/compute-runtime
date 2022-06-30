/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

using XeHPHwInfoConfig = ::testing::Test;

XEHPTEST_F(XeHPHwInfoConfig, givenConfigStringWhenSettingUpHardwareThenThrow) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, config));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

XEHPTEST_F(XeHPHwInfoConfig, givenXeHPMultiConfigWhenConfigureHardwareCustomIsCalledThenCapabilityTableIsSetProperly) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrE2ECompression = true;

    hwInfo.gtSystemInfo.EUCount = 256u;
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrRenderCompressedImages);

    hwInfo.gtSystemInfo.EUCount = 512u;
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(hwInfo.capabilityTable.ftrRenderCompressedImages);
}

XEHPTEST_F(XeHPHwInfoConfig, givenXeHPWhenConfiguringThenDisableRcs) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.featureTable.flags.ftrRcsNode);
}

XEHPTEST_F(XeHPHwInfoConfig, givenDebugVariableSetWhenConfiguringThenEnableRcs) {
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.featureTable.flags.ftrRcsNode);
}

XEHPTEST_F(XeHPHwInfoConfig, givenXeHpWhenCallingGetDeviceMemoryNameThenHbmIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    auto deviceMemoryName = hwInfoConfig->getDeviceMemoryName();
    EXPECT_TRUE(hasSubstr(deviceMemoryName, std::string("HBM")));
}

XEHPTEST_F(XeHPHwInfoConfig, givenA0OrA1SteppingWhenAskingIfExtraParametersAreInvalidThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, true},
         {REVISION_A1, true},
         {REVISION_B, false},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);

        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, hwInfoConfig->extraParametersInvalid(hwInfo));
    }
}

using XeHPHwHelperTest = HwHelperTest;

XEHPTEST_F(XeHPHwHelperTest, givenXeHPMultiConfigWhenAllowCompressionIsCalledThenCorrectValueIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.EUCount = 512u;
    EXPECT_TRUE(hwInfoConfig->allowCompression(hwInfo));

    hwInfo.gtSystemInfo.EUCount = 256u;
    EXPECT_FALSE(hwInfoConfig->allowCompression(hwInfo));
}

XEHPTEST_F(XeHPHwInfoConfig, givenHwInfoConfigWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_FALSE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));
}

XEHPTEST_F(XeHPHwInfoConfig, givenHwInfoConfigWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

XEHPTEST_F(XeHPHwInfoConfig, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

XEHPTEST_F(XeHPHwInfoConfig, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandDisabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}
