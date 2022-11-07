/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"

using namespace NEO;

using PvcHwInfoConfig = Test<DeviceFixture>;

PVCTEST_F(PvcHwInfoConfig, givenPVCRevId3AndAboveWhenGettingThreadEuRatioForScratchThen16IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 3;
    EXPECT_EQ(16u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo));

    hwInfo.platform.usRevId = 4;
    EXPECT_EQ(16u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo));
}

PVCTEST_F(PvcHwInfoConfig, givenPVCRevId0WhenGettingThreadEuRatioForScratchThen8IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0;
    EXPECT_EQ(8u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo));
}

PVCTEST_F(PvcHwInfoConfig, givenPVCWithDifferentSteppingsThenImplicitScalingIsEnabledForBAndHigher) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    auto hwInfo = *defaultHwInfo;

    for (uint32_t stepping = 0; stepping < 0x10; stepping++) {
        auto hwRevIdFromStepping = hwInfoConfig.getHwRevIdFromStepping(stepping, hwInfo);
        if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
            hwInfo.platform.usRevId = hwRevIdFromStepping;
            const bool shouldSupportImplicitScaling = hwRevIdFromStepping >= REVISION_B;
            EXPECT_EQ(shouldSupportImplicitScaling, hwInfoConfig.isImplicitScalingSupported(hwInfo)) << "hwRevId: " << hwRevIdFromStepping;
        }
    }
}

PVCTEST_F(PvcHwInfoConfig, givenPvcHwInfoWhenIsIpSamplingSupportedThenCorrectResultIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }
}

PVCTEST_F(PvcHwInfoConfig, givenHwInfoConfigWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionIfNecessaryFlagSupported());
}

PVCTEST_F(PvcHwInfoConfig, givenPvcHwInfoConfigWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_FALSE(hwInfoConfig.isPrefetcherDisablingInDirectSubmissionRequired());
}

PVCTEST_F(PvcHwInfoConfig, givenPvcHwInfoConfigWhenIsStatefulAddressingModeSupportedThenReturnFalse) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_FALSE(hwInfoConfig.isStatefulAddressingModeSupported());
}

PVCTEST_F(PvcHwInfoConfig, givenPvcSteppingWhenQueryIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenAppropriateValueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    hwInfo.platform.usRevId = 0x0;
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo));

    hwInfo.platform.usRevId = 0x6;
    EXPECT_TRUE(hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo));
}

PVCTEST_F(PvcHwInfoConfig, givenHwInfoConfigWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_TRUE(hwInfoConfig.getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(hwInfoConfig.getSbaPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(hwInfoConfig.getSbaPropertyStatelessMocsSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(hwInfoConfig.getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_FALSE(hwInfoConfig.getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertySystolicModeSupport());
}

PVCTEST_F(PvcHwInfoConfig, givenPvcWhenCallingGetDeviceMemoryNameThenHbmIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto deviceMemoryName = hwInfoConfig->getDeviceMemoryName();
    EXPECT_TRUE(hasSubstr(deviceMemoryName, std::string("HBM")));
}

PVCTEST_F(PvcHwInfoConfig, givenHwInfoConfigWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    hwInfoConfig.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_FALSE(fePropertiesSupport.disableOverdispatch);

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));

    hwInfoConfig.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

PVCTEST_F(PvcHwInfoConfig, givenHwInfoConfigWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isBasicWARequired);
    EXPECT_FALSE(isExtendedWARequired);
}

PVCTEST_F(PvcHwInfoConfig, givenPvcHwInfoConfigWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(hwInfoConfig.isDirectSubmissionSupported(hwInfo));
}

PVCTEST_F(PvcHwInfoConfig, givenPvcHwInfoConfigWhenCheckCopyEngineSelectorEnabledThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(hwInfoConfig.isCopyEngineSelectorEnabled(hwInfo));
}

PVCTEST_F(PvcHwInfoConfig, givenHwInfoConfigAndProgramExtendedPipeControlPriorToNonPipelinedStateCommandDisabledWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenFalseIsReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_FALSE(isBasicWARequired);
}

PVCTEST_F(PvcHwInfoConfig, givenPvcWhenConfiguringThenDisableCccs) {
    auto &hwInfoConfig = getHwInfoConfig();
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.featureTable.flags.ftrRcsNode);
}

PVCTEST_F(PvcHwInfoConfig, givenDebugVariableSetWhenConfiguringThenEnableCccs) {
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    auto &hwInfoConfig = getHwInfoConfig();
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.featureTable.flags.ftrRcsNode);
}

PVCTEST_F(PvcHwInfoConfig, givenDeviceIdThenProperMaxThreadsForWorkgroupIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_EQ(64u, hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, 64u, 64u));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        uint32_t numThreadsPerEU = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
        EXPECT_EQ(64u * numThreadsPerEU, hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, 64u, 64u));
    }
}

PVCTEST_F(PvcHwInfoConfig, givenVariousValuesWhenConvertingHwRevIdAndSteppingThenConversionIsCorrect) {
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    for (uint32_t testValue = 0; testValue < 0xFF; testValue++) {
        for (const auto *pvc : {&pvcXlDeviceIds, &pvcXtDeviceIds}) {
            for (const auto &deviceId : *pvc) {
                hwInfo.platform.usDeviceID = deviceId;
                auto hwRevIdFromStepping = hwInfoConfig.getHwRevIdFromStepping(testValue, hwInfo);
                if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
                    hwInfo.platform.usRevId = hwRevIdFromStepping;
                    EXPECT_EQ(testValue, hwInfoConfig.getSteppingFromHwRevId(hwInfo));
                }
            }
        }
        hwInfo.platform.usRevId = testValue;
        auto steppingFromHwRevId = hwInfoConfig.getSteppingFromHwRevId(hwInfo);
        if (steppingFromHwRevId != CommonConstants::invalidStepping) {
            bool anyMatchAfterConversionFromStepping = false;
            for (const auto *pvc : {&pvcXlDeviceIds, &pvcXtDeviceIds}) {
                for (const auto &deviceId : *pvc) {
                    hwInfo.platform.usDeviceID = deviceId;
                    auto hwRevId = hwInfoConfig.getHwRevIdFromStepping(steppingFromHwRevId, hwInfo);
                    EXPECT_NE(CommonConstants::invalidStepping, hwRevId);
                    // expect values to match. 0x1 and 0x0 translate to the same stepping so they are interpreted as a match too.
                    if (((testValue & PVC::pvcSteppingBits) == (hwRevId & PVC::pvcSteppingBits)) ||
                        (((testValue & PVC::pvcSteppingBits) == 0x1) && ((hwRevId & PVC::pvcSteppingBits) == 0x0))) {
                        anyMatchAfterConversionFromStepping = true;
                    }
                }
            }
            EXPECT_TRUE(anyMatchAfterConversionFromStepping);
        }
    }
}

PVCTEST_F(PvcHwInfoConfig, givenPvcHwInfoConfigWhenIsIpSamplingSupportedThenCorrectResultIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }
}
