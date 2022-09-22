/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

using PvcHwInfoConfig = ::testing::Test;

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

using PvcHwInfo = ::testing::Test;

PVCTEST_F(PvcHwInfo, givenPvcWhenConfiguringThenDisableCccs) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.featureTable.flags.ftrRcsNode);
}

PVCTEST_F(PvcHwInfo, givenDebugVariableSetWhenConfiguringThenEnableCccs) {
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.featureTable.flags.ftrRcsNode);
}

PVCTEST_F(PvcHwInfo, givenDeviceIdThenProperMaxThreadsForWorkgroupIsReturned) {
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

PVCTEST_F(PvcHwInfo, givenVariousValuesWhenConvertingHwRevIdAndSteppingThenConversionIsCorrect) {
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

PVCTEST_F(PvcHwInfo, givenPvcHwInfoConfigWhenIsIpSamplingSupportedThenCorrectResultIsReturned) {
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
