/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/product_config_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe_hpg_core/dg2/product_configs_dg2.h"

using namespace NEO;

using HwInfoConfigTestDg2 = ::testing::Test;

DG2TEST_F(HwInfoConfigTestDg2, whenConvertingTimestampsToCsDomainThenGpuTicksAreShifted) {
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    uint64_t gpuTicks = 0x12345u;

    auto expectedGpuTicks = gpuTicks >> 1;

    hwInfoConfig->convertTimestampsFromOaToCsDomain(gpuTicks);
    EXPECT_EQ(expectedGpuTicks, gpuTicks);
}

DG2TEST_F(HwInfoConfigTestDg2, givenDg2ConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    Dg2HwConfig::setupHardwareInfoBase(&hwInfo, false);

    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

DG2TEST_F(HwInfoConfigTestDg2, givenDg2ConfigWhenSetupHardwareInfoThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    Dg2HwConfig::setupHardwareInfo(&hwInfo, false);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoErrorneousConfigStringThenThrow) {
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

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoConfigWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));
}

DG2TEST_F(HwInfoConfigTestDg2, whenAdjustingDefaultEngineTypeThenSelectEngineTypeBasedOnRevisionId) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    hardwareInfo.capabilityTable.defaultEngineType = defaultHwInfo->capabilityTable.defaultEngineType;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    hwHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);

    hardwareInfo.capabilityTable.defaultEngineType = defaultHwInfo->capabilityTable.defaultEngineType;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hardwareInfo);
    hwHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG2TEST_F(HwInfoConfigTestDg2, givenA0OrA1SteppingWhenAskingIfWAIsRequiredThenReturnTrue) {
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

        EXPECT_EQ(paramBool, hwInfoConfig->isDefaultEngineTypeAdjustmentRequired(hwInfo));
        EXPECT_EQ(paramBool, hwInfoConfig->isAllocationSizeAdjustmentRequired(hwInfo));
        EXPECT_EQ(paramBool, hwInfoConfig->isPrefetchDisablingRequired(hwInfo));
    }
}

DG2TEST_F(HwInfoConfigTestDg2, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenProgramPipeControlPriorToNonPipelinedStateCommandDisabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoConfigWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoConfigWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoConfigWithSingleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoConfigWithSingleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(HwInfoConfigTestDg2, givenDg2WhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_TRUE(hwInfoConfig.isBlitterForImagesSupported());
}

DG2TEST_F(HwInfoConfigTestDg2, givenB0rCSteppingWhenAskingIfTile64With3DSurfaceOnBCSIsSupportedThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, false},
         {REVISION_A1, false},
         {REVISION_B, false},
         {REVISION_C, true}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);

        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, hwInfoConfig->isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(HwInfoConfigTestDg2, givenA0SteppingAnd128EuWhenConfigureCalledThenDisableCompression) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    for (uint8_t revision : {REVISION_A0, REVISION_A1}) {
        for (uint32_t euCount : {127, 128, 129}) {
            HardwareInfo hwInfo = *defaultHwInfo;
            hwInfo.featureTable.flags.ftrE2ECompression = true;

            hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.gtSystemInfo.EUCount = euCount;

            hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

            auto compressionExpected = (euCount == 128) ? true : (revision != REVISION_A0);

            EXPECT_EQ(compressionExpected, hwInfo.capabilityTable.ftrRenderCompressedBuffers);
            EXPECT_EQ(compressionExpected, hwInfo.capabilityTable.ftrRenderCompressedImages);
            EXPECT_EQ(compressionExpected, hwInfoConfig->allowCompression(hwInfo));
        }
    }
}

DG2TEST_F(HwInfoConfigTestDg2, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    auto hardwareInfo = *defaultHwInfo;
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

DG2TEST_F(HwInfoConfigTestDg2, givenRevisionEnumAndDisableL3CacheForDebugCalledThenCorrectValueIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    auto hardwareInfo = *defaultHwInfo;
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

DG2TEST_F(HwInfoConfigTestDg2, givenDg2WhenSetForceNonCoherentThenProperFlagSet) {
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

DG2TEST_F(HwInfoConfigTestDg2, givenEnabledSliceInNonStandardConfigWhenComputeUnitsUsedForScratchThenProperCalculationIsReturned) {
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

DG2TEST_F(HwInfoConfigTestDg2, givenNotEnabledSliceWhenComputeUnitsUsedForScratchThenThrowUnrecoverableIf) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    for (int i = 0; i < GT_MAX_SLICE; i++) {
        testSysInfo.SliceInfo[i].Enabled = false;
    }

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    EXPECT_THROW(hwHelper.getComputeUnitsUsedForScratch(&hwInfo), std::exception);
}

HWTEST_EXCLUDE_PRODUCT(HwHelperTestXeHpgCore, givenHwHelperWhenCheckTimestampWaitSupportThenReturnFalse, IGFX_DG2);
DG2TEST_F(HwInfoConfigTestDg2, givenDG2WhenCheckingIsTimestampWaitSupportedForEventsThenReturnTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_TRUE(hwInfoConfig.isTimestampWaitSupportedForEvents());
    EXPECT_TRUE(hwHelper.isTimestampWaitSupportedForEvents(hwInfo));
}

DG2TEST_F(ProductConfigTests, givenDg2G10DeviceIdsWhenConfigIsCheckedThenCorrectValueIsReturned) {
    for (const auto &deviceId : dg2G10DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        EXPECT_TRUE(DG2::isG10(hwInfo));
        EXPECT_FALSE(DG2::isG11(hwInfo));
        EXPECT_FALSE(DG2::isG12(hwInfo));
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G11DeviceIdsWhenConfigIsCheckedThenCorrectValueIsReturned) {
    for (const auto &deviceId : dg2G11DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        EXPECT_FALSE(DG2::isG10(hwInfo));
        EXPECT_TRUE(DG2::isG11(hwInfo));
        EXPECT_FALSE(DG2::isG12(hwInfo));
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G12DeviceIdsWhenConfigIsCheckedThenCorrectValueIsReturned) {
    for (const auto &deviceId : dg2G12DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        EXPECT_FALSE(DG2::isG10(hwInfo));
        EXPECT_FALSE(DG2::isG11(hwInfo));
        EXPECT_TRUE(DG2::isG12(hwInfo));
    }
}

DG2TEST_F(ProductConfigTests, givenInvalidRevisionIdWhenDeviceIdIsDefaultThenUnknownIsaIsReturned) {
    hwInfo.platform.usDeviceID = 0;
    hwInfo.platform.usRevId = CommonConstants::invalidRevisionID;

    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
}

DG2TEST_F(ProductConfigTests, givenDg2G10DeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : dg2G10DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_A0);

        hwInfo.platform.usRevId = 0x1;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_A1);

        hwInfo.platform.usRevId = 0x4;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_B0);

        hwInfo.platform.usRevId = 0x8;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_C0);
    }
}

DG2TEST_F(ProductConfigTests, givenDg2DeviceIdWhenIncorrectRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &dg2 : {dg2G10DeviceIds, dg2G11DeviceIds}) {
        for (const auto &deviceId : dg2) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = CommonConstants::invalidRevisionID;
            productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
            EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
        }
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G11DeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : dg2G11DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_A0);

        hwInfo.platform.usRevId = 0x4;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_B0);

        hwInfo.platform.usRevId = 0x5;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_B1);
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G12DeviceIdWhenGetProductConfigThenCorrectConfigIsReturned) {
    for (const auto &deviceId : dg2G12DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G12_A0);
    }
}

DG2TEST_F(ProductConfigTests, givenNotSetDeviceAndRevisionIdWhenGetProductConfigThenUnknownIsaIsReturned) {
    hwInfo.platform.usRevId = 0x0;
    hwInfo.platform.usDeviceID = 0x0;

    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
}

DG2TEST_F(HwInfoConfigTestDg2, givenHwInfoConfigWhenAskedIfStorageInfoAdjustmentIsRequiredThenTrueIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    if constexpr (is32bit) {
        EXPECT_TRUE(hwInfoConfig->isStorageInfoAdjustmentRequired());
    } else {
        EXPECT_FALSE(hwInfoConfig->isStorageInfoAdjustmentRequired());
    }
}