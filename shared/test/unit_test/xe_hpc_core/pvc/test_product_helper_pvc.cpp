/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "gtest/gtest.h"

using namespace NEO;

using PvcProductHelper = ProductHelperTest;

PVCTEST_F(PvcProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Pvc, productHelper->getAubStreamProductFamily());
}

PVCTEST_F(PvcProductHelper, whenCheckIsTlbFlushRequiredThenReturnProperValue) {
    EXPECT_FALSE(productHelper->isTlbFlushRequired());
}

PVCTEST_F(PvcProductHelper, whenForceTlbFlushSetAndCheckIsTlbFlushRequiredThenReturnProperValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceTlbFlush.set(1);
    EXPECT_TRUE(productHelper->isTlbFlushRequired());
}

PVCTEST_F(PvcProductHelper, givenPVCRevId3AndAboveWhenGettingThreadEuRatioForScratchThen16IsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 3;
    EXPECT_EQ(16u, productHelper->getThreadEuRatioForScratch(hwInfo));

    hwInfo.platform.usRevId = 4;
    EXPECT_EQ(16u, productHelper->getThreadEuRatioForScratch(hwInfo));
}

PVCTEST_F(PvcProductHelper, givenPVCRevId0WhenGettingThreadEuRatioForScratchThen8IsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0;
    EXPECT_EQ(8u, productHelper->getThreadEuRatioForScratch(hwInfo));
}

PVCTEST_F(PvcProductHelper, givenPVCWithDifferentSteppingsThenImplicitScalingIsEnabledForBAndHigher) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = pvcXlDeviceIds[0];

    for (uint32_t stepping = 0; stepping < 0x10; stepping++) {
        auto hwRevIdFromStepping = productHelper->getHwRevIdFromStepping(stepping, hwInfo);
        if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
            hwInfo.platform.usRevId = hwRevIdFromStepping;
            const bool shouldSupportImplicitScaling = hwRevIdFromStepping >= REVISION_B;
            EXPECT_EQ(shouldSupportImplicitScaling, productHelper->isImplicitScalingSupported(hwInfo)) << "hwRevId: " << hwRevIdFromStepping;
        }
    }
}

PVCTEST_F(PvcProductHelper, givenPvcHwInfoWhenIsIpSamplingSupportedThenCorrectResultIsReturned) {
    auto hwInfo = *defaultHwInfo;

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(productHelper->isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(productHelper->isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtVgDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(productHelper->isIpSamplingSupported(hwInfo));
    }
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {

    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->isPrefetcherDisablingInDirectSubmissionRequired());
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenIsStatefulAddressingModeSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isStatefulAddressingModeSupported());
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenCheckIsCopyBufferRectSplitSupportedThenReturnsFalse) {
    EXPECT_FALSE(productHelper->isCopyBufferRectSplitSupported());
}

PVCTEST_F(PvcProductHelper, givenPvcSteppingWhenQueryIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenAppropriateValueIsReturned) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.platform.usRevId = 0x0;
    EXPECT_FALSE(productHelper->isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo));

    hwInfo.platform.usRevId = 0x6;
    EXPECT_TRUE(productHelper->isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo));
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_TRUE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_TRUE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_FALSE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_TRUE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, hwInfo);

    EXPECT_FALSE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_FALSE(fePropertiesSupport.disableOverdispatch);

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

    EXPECT_FALSE(isBasicWARequired);
    EXPECT_FALSE(isExtendedWARequired);
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenCheckDirectSubmissionConstantCacheInvalidationNeededThenFalseIsReturned) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(productHelper->isDirectSubmissionConstantCacheInvalidationNeeded(hwInfo));
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenCheckCopyEngineSelectorEnabledThenFalseIsReturned) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(productHelper->isCopyEngineSelectorEnabled(hwInfo));
}

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenIsSkippingStatefulInformationRequiredThenCorrectResultIsReturned) {

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelMetadata.isGeneratedByIgc = true;
    EXPECT_FALSE(productHelper->isSkippingStatefulInformationRequired(kernelDescriptor));

    kernelDescriptor.kernelMetadata.isGeneratedByIgc = false;
    EXPECT_TRUE(productHelper->isSkippingStatefulInformationRequired(kernelDescriptor));
}

PVCTEST_F(PvcProductHelper, givenProductHelperAndProgramExtendedPipeControlPriorToNonPipelinedStateCommandDisabledWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenFalseIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_FALSE(isBasicWARequired);
}

PVCTEST_F(PvcProductHelper, givenDeviceIdThenProperMaxThreadsForWorkgroupIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_EQ(64u, productHelper->getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, 64u, 64u));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        uint32_t numThreadsPerEU = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
        EXPECT_EQ(64u * numThreadsPerEU, productHelper->getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, 64u, 64u));
    }

    for (const auto &deviceId : pvcXtVgDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        uint32_t numThreadsPerEU = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
        EXPECT_EQ(64u * numThreadsPerEU, productHelper->getMaxThreadsForWorkgroupInDSSOrSS(hwInfo, 64u, 64u));
    }
}

PVCTEST_F(PvcProductHelper, givenVariousValuesWhenConvertingHwRevIdAndSteppingThenConversionIsCorrect) {
    auto hwInfo = *defaultHwInfo;

    for (uint32_t testValue = 0; testValue < 0xFF; testValue++) {
        for (const auto *pvc : {&pvcXlDeviceIds, &pvcXtDeviceIds, &pvcXtVgDeviceIds}) {
            for (const auto &deviceId : *pvc) {
                hwInfo.platform.usDeviceID = deviceId;
                auto hwRevIdFromStepping = productHelper->getHwRevIdFromStepping(testValue, hwInfo);
                if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
                    hwInfo.platform.usRevId = hwRevIdFromStepping;
                    EXPECT_EQ(testValue, productHelper->getSteppingFromHwRevId(hwInfo));
                }
            }
        }
        hwInfo.platform.usRevId = testValue;
        auto steppingFromHwRevId = productHelper->getSteppingFromHwRevId(hwInfo);
        if (steppingFromHwRevId != CommonConstants::invalidStepping) {
            bool anyMatchAfterConversionFromStepping = false;
            for (const auto *pvc : {&pvcXlDeviceIds, &pvcXtDeviceIds, &pvcXtVgDeviceIds}) {
                for (const auto &deviceId : *pvc) {
                    hwInfo.platform.usDeviceID = deviceId;
                    auto hwRevId = productHelper->getHwRevIdFromStepping(steppingFromHwRevId, hwInfo);
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

PVCTEST_F(PvcProductHelper, givenPvcProductHelperWhenIsIpSamplingSupportedThenCorrectResultIsReturned) {
    auto hwInfo = *defaultHwInfo;

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(productHelper->isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(productHelper->isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtVgDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(productHelper->isIpSamplingSupported(hwInfo));
    }
}

PVCTEST_F(PvcProductHelper, whenQueryingMaxNumSamplersThenReturnZero) {
    EXPECT_EQ(0u, productHelper->getMaxNumSamplers());
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenAskingForReadOnlyResourceSupportThenTrueReturned) {
    EXPECT_TRUE(productHelper->supportReadOnlyAllocations());
}

PVCTEST_F(PvcProductHelper, givenProductHelperWhenGetRequiredDetectIndirectVersionCalledThenReturnCorrectVersion) {
    EXPECT_EQ(3u, productHelper->getRequiredDetectIndirectVersion());
    EXPECT_EQ(9u, productHelper->getRequiredDetectIndirectVersionVC());
}

PVCTEST_F(ProductHelperTest, givenProductHelperWhenAskingForSharingWith3dOrMediaSupportThenFalseReturned) {
    EXPECT_FALSE(productHelper->isSharingWith3dOrMediaAllowed());
}

PVCTEST_F(ProductHelperTest, givenProductHelperThenCompressionIsForbidden) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isCompressionForbidden(hwInfo));
}
