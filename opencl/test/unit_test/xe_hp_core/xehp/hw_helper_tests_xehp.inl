/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestsXeHP = HwHelperTest;

HWTEST_EXCLUDE_PRODUCT(HwHelperTest, WhenIsBankOverrideRequiredIsCalledThenFalseIsReturned, IGFX_XE_HP_SDV);

XEHPTEST_F(HwHelperTestsXeHP, givenXEHPWhenIsBankOverrideRequiredIsCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;
    auto &helper = HwHelper::get(renderCoreFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;

    {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 4;
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_TRUE(helper.isBankOverrideRequired(hwInfo));
    }
    {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 4;
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_B, hwInfo);
        EXPECT_FALSE(helper.isBankOverrideRequired(hwInfo));
    }
    {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 2;
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(helper.isBankOverrideRequired(hwInfo));
    }
    {
        DebugManager.flags.ForceMemoryBankIndexOverride.set(1);
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 1;
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_TRUE(helper.isBankOverrideRequired(hwInfo));
    }
    {
        DebugManager.flags.ForceMemoryBankIndexOverride.set(0);
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 4;
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(helper.isBankOverrideRequired(hwInfo));
    }
}

XEHPTEST_F(HwHelperTestsXeHP, givenXEHPWhenHeapInLocalMemIsCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;
    auto &helper = HwHelper::get(renderCoreFamily);
    auto hwInfo = *defaultHwInfo;

    {
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(helper.heapInLocalMem(hwInfo));
    }
    {
        hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_B, hwInfo);
        EXPECT_TRUE(helper.heapInLocalMem(hwInfo));
    }
}

XEHPTEST_F(HwHelperTestsXeHP, givenRcsDisabledWhenGetGpgpuEnginesCalledThenDontSetRcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.ftrRcsNode = false;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(8u, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(8u, engines.size());

    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[0].first); // low priority
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[1].first); // internal
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[5].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);
}

XEHPTEST_F(HwHelperTestsXeHP, givenRcsDisabledButDebugVariableSetWhenGetGpgpuEnginesCalledThenSetRcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.ftrRcsNode = false;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(9u, device->engines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(9u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first); // low priority
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first); // internal
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[5].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[8].first);
}

XEHPTEST_F(HwHelperTestsXeHP, GivenVariousValuesWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto &hwInfo = pDevice->getHardwareInfo();

    for (auto &testInput : computeSlmValuesXeHPPlusTestsInput) {
        EXPECT_EQ(testInput.expected, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, testInput.slmSize));
    }
}
