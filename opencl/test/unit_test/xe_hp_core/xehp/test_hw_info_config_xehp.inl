/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

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
    hwInfo.featureTable.ftrE2ECompression = true;

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
    EXPECT_FALSE(hwInfo.featureTable.ftrRcsNode);
}

XEHPTEST_F(XeHPHwInfoConfig, givenDebugVariableSetWhenConfiguringThenEnableRcs) {
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.featureTable.ftrRcsNode);
}

using XeHPHwHelperTest = HwHelperTest;

XEHPTEST_F(XeHPHwHelperTest, givenXeHPMultiConfigWhenAllowRenderCompressionIsCalledThenCorrectValueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.EUCount = 512u;
    EXPECT_TRUE(helper.allowRenderCompression(hwInfo));

    hwInfo.gtSystemInfo.EUCount = 256u;
    EXPECT_FALSE(helper.allowRenderCompression(hwInfo));
}
