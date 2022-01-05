/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestsDg2 = HwHelperTest;

DG2TEST_F(HwHelperTestsDg2, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping <= REVISION_B) {
            if (stepping == REVISION_A0) {
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
            } else if (stepping == REVISION_A1) {
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
            } else { // REVISION_B
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
            }
        } else {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        }

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A1, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_A1, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo));

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo));
    }
}

DG2TEST_F(HwHelperTestsDg2, givenRevisionEnumAndDisableL3CacheForDebugCalledThenCorrectValueIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);
        if (stepping < REVISION_B) {
            EXPECT_TRUE(hwHelper.disableL3CacheForDebug(hardwareInfo));
        } else {
            EXPECT_FALSE(hwHelper.disableL3CacheForDebug(hardwareInfo));
        }
    }
}

DG2TEST_F(HwHelperTestsDg2, givenDg2WhenSetForceNonCoherentThenProperFlagSet) {
    using FORCE_NON_COHERENT = typename FamilyType::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    auto stateComputeMode = FamilyType::cmdInitStateComputeMode;
    auto properties = StateComputeModeProperties{};

    properties.isCoherencyRequired.set(false);
    hwInfoConfig->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(XE_HPG_COREFamily::stateComputeModeForceNonCoherentMask, stateComputeMode.getMaskBits());

    properties.isCoherencyRequired.set(true);
    hwInfoConfig->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(XE_HPG_COREFamily::stateComputeModeForceNonCoherentMask, stateComputeMode.getMaskBits());
}

DG2TEST_F(HwHelperTestsDg2, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 7, 1), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterDeviceCapsTests, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue, IGFX_DG2);
DG2TEST_F(HwHelperTestsDg2, givenEnabledSliceInNonStandardConfigWhenComputeUnitsUsedForScratchThenProperCalculationIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    for (int i = 0; i < GT_MAX_SLICE; i++) {
        testSysInfo.SliceInfo[i].Enabled = false;
    }
    testSysInfo.SliceInfo[2].Enabled = true;
    testSysInfo.SliceInfo[3].Enabled = true;
    auto highestEnabledSlice = 4;
    auto subSlicesPerSlice = testSysInfo.MaxSubSlicesSupported / testSysInfo.MaxSlicesSupported;
    auto maxSubSlice = highestEnabledSlice * subSlicesPerSlice;

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    uint32_t expectedCalculation = maxSubSlice * testSysInfo.MaxEuPerSubSlice * (testSysInfo.ThreadCount / testSysInfo.EUCount);

    EXPECT_EQ(expectedCalculation, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
}

DG2TEST_F(HwHelperTestsDg2, givenNotEnabledSliceWhenComputeUnitsUsedForScratchThenThrowUnrecoverableIf) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    for (int i = 0; i < GT_MAX_SLICE; i++) {
        testSysInfo.SliceInfo[i].Enabled = false;
    }

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    EXPECT_THROW(hwHelper.getComputeUnitsUsedForScratch(&hwInfo), std::exception);
}
