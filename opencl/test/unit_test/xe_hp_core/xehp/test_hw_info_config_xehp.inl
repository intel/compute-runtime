/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

using XeHPProductHelper = Test<DeviceFixture>;

XEHPTEST_F(XeHPProductHelper, givenXeHPMultiConfigWhenConfigureHardwareCustomIsCalledThenCapabilityTableIsSetProperly) {
    auto &productHelper = getHelper<ProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrE2ECompression = true;

    hwInfo.gtSystemInfo.EUCount = 256u;
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrRenderCompressedImages);

    hwInfo.gtSystemInfo.EUCount = 512u;
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(hwInfo.capabilityTable.ftrRenderCompressedImages);
}

XEHPTEST_F(XeHPProductHelper, givenXeHPWhenConfiguringThenDisableRcs) {
    auto &productHelper = getHelper<ProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;

    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.featureTable.flags.ftrRcsNode);
}

XEHPTEST_F(XeHPProductHelper, givenDebugVariableSetWhenConfiguringThenEnableRcs) {
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    auto &productHelper = getHelper<ProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;

    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.featureTable.flags.ftrRcsNode);
}

XEHPTEST_F(XeHPProductHelper, givenXeHpWhenCallingGetDeviceMemoryNameThenHbmIsReturned) {
    auto productHelper = ProductHelper::get(productFamily);
    auto deviceMemoryName = productHelper->getDeviceMemoryName();
    EXPECT_TRUE(hasSubstr(deviceMemoryName, std::string("HBM")));
}

XEHPTEST_F(XeHPProductHelper, givenA0OrA1SteppingWhenAskingIfExtraParametersAreInvalidThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, true},
         {REVISION_A1, true},
         {REVISION_B, false},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper.extraParametersInvalid(hwInfo));
    }
}

using XeHPGfxCoreHelperTest = GfxCoreHelperTest;

XEHPTEST_F(XeHPGfxCoreHelperTest, givenXeHPMultiConfigWhenAllowCompressionIsCalledThenCorrectValueIsReturned) {
    auto productHelper = ProductHelper::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.EUCount = 512u;
    EXPECT_TRUE(productHelper->allowCompression(hwInfo));

    hwInfo.gtSystemInfo.EUCount = 256u;
    EXPECT_FALSE(productHelper->allowCompression(hwInfo));
}

XEHPTEST_F(XeHPProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_FALSE(productHelper.isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_FALSE(fePropertiesSupport.disableOverdispatch);

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(hwInfo));

    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

XEHPTEST_F(XeHPProductHelper, givenProductHelperWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledThenTrueIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

XEHPTEST_F(XeHPProductHelper, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

XEHPTEST_F(XeHPProductHelper, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandDisabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}
