/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "platforms.h"

using namespace NEO;

using XeHpProductHelper = ProductHelperTest;

XEHPTEST_F(XeHpProductHelper, givenXEHPWithA0SteppingThenMaxThreadsForWorkgroupWAIsRequired) {

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, hwInfo);
    auto isWARequired = productHelper->isMaxThreadsForWorkgroupWARequired(hwInfo);
    EXPECT_TRUE(isWARequired);
}

XEHPTEST_F(XeHpProductHelper, givenXEHPWithBSteppingThenMaxThreadsForWorkgroupWAIsNotRequired) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
    auto isWARequired = productHelper->isMaxThreadsForWorkgroupWARequired(hwInfo);
    EXPECT_FALSE(isWARequired);
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenRevisionIsAtLeastBThenAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(1);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    for (auto revision : {REVISION_A0, REVISION_A1, REVISION_B}) {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        if (revision < REVISION_B) {
            EXPECT_FALSE(productHelper->allowStatelessCompression(hwInfo));
        } else {
            EXPECT_TRUE(productHelper->allowStatelessCompression(hwInfo));
        }
    }
}

XEHPTEST_F(XeHpProductHelper, givenXeHpProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

XEHPTEST_F(XeHpProductHelper, givenXeHpCoreProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo;

    {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(productHelper->isDirectSubmissionSupported(hwInfo));
    }

    {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A1, hwInfo);
        EXPECT_FALSE(productHelper->isDirectSubmissionSupported(hwInfo));
    }

    {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
        EXPECT_TRUE(productHelper->isDirectSubmissionSupported(hwInfo));
    }
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenCreateMultipleSubDevicesThenDontAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_FALSE(productHelper->allowStatelessCompression(hwInfo));
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenCreateMultipleSubDevicesAndEnableMultitileCompressionThenAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.EnableMultiTileCompression.set(1);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(productHelper->allowStatelessCompression(hwInfo));
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenCompressedBuffersAreDisabledThenDontAllowStatelessCompression) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    EXPECT_FALSE(productHelper->allowStatelessCompression(pInHwInfo));
}

XEHPTEST_F(XeHpProductHelper, givenSteppingWhenAskingForLocalMemoryAccessModeThenDisallowOnA0) {
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, productHelper->getLocalMemoryAccessMode(hwInfo));

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::Default, productHelper->getLocalMemoryAccessMode(hwInfo));
}

XEHPTEST_F(XeHpProductHelper, givenXEHPWhenHeapInLocalMemIsCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;
    auto hwInfo = *defaultHwInfo;

    {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(productHelper->heapInLocalMem(hwInfo));
    }
    {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
        EXPECT_TRUE(productHelper->heapInLocalMem(hwInfo));
    }
}

XEHPTEST_F(XeHpProductHelper, givenXeHpCoreWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {

    EXPECT_TRUE(productHelper->isBlitterForImagesSupported());
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenIsImplicitScalingSupportedThenExpectTrue) {

    EXPECT_TRUE(productHelper->isImplicitScalingSupported(*defaultHwInfo));
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {

    EXPECT_EQ(productHelper->getHwIpVersion(*defaultHwInfo), AOT::XEHP_SDV);
}

XEHPTEST_F(XeHpProductHelper, givenProductHelperWhenIsSystolicModeConfigurabledThenTrueIsReturned) {

    EXPECT_TRUE(productHelper->isSystolicModeConfigurable(*defaultHwInfo));
}

XEHPTEST_F(XeHpProductHelper, whenGettingDefaultRevisionThenB0IsReturned) {
    EXPECT_EQ(productHelper->getHwRevIdFromStepping(REVISION_B, *defaultHwInfo), productHelper->getDefaultRevisionId());
}
