/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "gtest/gtest.h"
#include "neo_aot_platforms.h"

#include <array>

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
}

using namespace NEO;
using ProductConfigHelperDg2Tests = ::testing::Test;
using ProductHelperTestDg2 = ProductHelperTest;

DG2TEST_F(ProductConfigHelperDg2Tests, givenVariousVariantsOfXeHpgAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    std::vector<std::string> acronymsVariants = {"xe_hpg_core", "xe_hpg", "xehpg", "XeHpg"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = productConfigHelper->getReleaseFromDeviceName(acronym);
        EXPECT_EQ(ret, AOT::XE_HPG_RELEASE);
    }
}

DG2TEST_F(ProductConfigHelperDg2Tests, givenXeHpgReleaseWhenSearchForDeviceAcronymThenObjectIsFound) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    auto aotInfos = productConfigHelper->getDeviceAotInfo();
    EXPECT_TRUE(std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(AOT::XE_HPG_RELEASE)));
}
DG2TEST_F(ProductHelperTestDg2, givenNoDpasInstructionInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const uint32_t lws[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {5, 3, 1};
    bool dpasInstruction = false;
    EXPECT_FALSE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount, hwInfo));
}

DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionAndG10Dg2DeviceWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = *dg2G10DeviceIds.begin();
    bool dpasInstruction = true;
    const uint32_t groupCount[3] = {5, 3, 1};
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, nullptr, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionAndG11Dg2DeviceWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = *dg2G11DeviceIds.begin();
    bool dpasInstruction = true;
    const uint32_t groupCount[3] = {5, 3, 1};
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, nullptr, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionAndG12Dg2DeviceWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = *dg2G12DeviceIds.begin();
    bool dpasInstruction = true;
    const uint32_t groupCount[3] = {5, 3, 1};
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, nullptr, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionAndNotAcmDeviceWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = 0x1234;
    bool dpasInstruction = true;
    const uint32_t groupCount[3] = {5, 3, 1};
    EXPECT_FALSE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, nullptr, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionLwsAndGroupCountIsNullPtrInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    bool dpasInstruction = true;
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, nullptr, nullptr, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionLwsIsNullPtrInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    bool dpasInstruction = true;
    const uint32_t groupCount[3] = {5, 3, 1};
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, nullptr, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionGroupCountIsNullPtrInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    bool dpasInstruction = true;
    const uint32_t lws[3] = {1, 1, 1};
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, lws, nullptr, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionLwsAndLwsIsOddWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const uint32_t lws[3] = {7, 3, 1};
    const uint32_t groupCount[3] = {2, 1, 1};
    bool dpasInstruction = true;
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionLwsAndLwsIsNoOddWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const uint32_t lws[3] = {8, 3, 1};
    const uint32_t groupCount[3] = {2, 1, 1};
    bool dpasInstruction = true;
    EXPECT_FALSE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionLwsAndLwsIsOneAndXGroupCountIsOddWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const uint32_t lws[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {5, 1, 1};
    bool dpasInstruction = true;
    EXPECT_TRUE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenDpasInstructionLwsAndLwsIsOneAndXGroupCountIsNoOddWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const uint32_t lws[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {4, 1, 1};
    bool dpasInstruction = true;
    EXPECT_FALSE(productHelper->isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount, hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenG10Dg2ProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = *dg2G10DeviceIds.begin();
    EXPECT_TRUE(productHelper->isCalculationForDisablingEuFusionWithDpasNeeded(hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenG11Dg2ProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = *dg2G11DeviceIds.begin();
    EXPECT_TRUE(productHelper->isCalculationForDisablingEuFusionWithDpasNeeded(hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenG12Dg2ProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = *dg2G12DeviceIds.begin();
    EXPECT_TRUE(productHelper->isCalculationForDisablingEuFusionWithDpasNeeded(hwInfo));
}
DG2TEST_F(ProductHelperTestDg2, givenNotACMProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = 0x1234;
    EXPECT_FALSE(productHelper->isCalculationForDisablingEuFusionWithDpasNeeded(hwInfo));
}

DG2TEST_F(ProductHelperTestDg2, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Dg2, productHelper->getAubStreamProductFamily());
}

DG2TEST_F(ProductHelperTestDg2, givenDg2ConfigWhenSetupHardwareInfoThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    Dg2HwConfig::setupHardwareInfo(&hwInfo, false, releaseHelper);

    EXPECT_EQ(0u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(0u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
}

DG2TEST_F(ProductHelperTestDg2, givenDg2ProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnFALSE) {
    EXPECT_FALSE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenCheckIsCopyBufferRectSplitSupportedThenReturnsFalse) {
    EXPECT_FALSE(productHelper->isCopyBufferRectSplitSupported());
}

DG2TEST_F(ProductHelperTestDg2, givenG10DevIdWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    myHwInfo.platform.usDeviceID = dg2G10DeviceIds[0];
    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, myHwInfo);
    EXPECT_FALSE(productHelper->isDisableOverdispatchAvailable(myHwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_FALSE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));

    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

DG2TEST_F(ProductHelperTestDg2, givenG11DevIdWhenIsDisableOverdispatchAvailableCalledThenTrueReturnedForAllSteppings) {
    FrontEndPropertiesSupport fePropertiesSupport{};
    HardwareInfo myHwInfo = *defaultHwInfo;

    myHwInfo.platform.usDeviceID = dg2G11DeviceIds[0];
    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_C, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

DG2TEST_F(ProductHelperTestDg2, givenG12DevIdWhenIsDisableOverdispatchAvailableCalledThenTrueReturnedForAllSteppings) {
    FrontEndPropertiesSupport fePropertiesSupport{};
    HardwareInfo myHwInfo = *defaultHwInfo;

    myHwInfo.platform.usDeviceID = dg2G12DeviceIds[0];
    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_C, myHwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(myHwInfo));
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

DG2TEST_F(ProductHelperTestDg2, whenAdjustingDefaultEngineTypeThenSelectEngineTypeBasedOnRevisionId) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.capabilityTable.defaultEngineType = defaultHwInfo->capabilityTable.defaultEngineType;
            gfxCoreHelper.adjustDefaultEngineType(&hwInfo, productHelper, nullptr);
            if (DG2::isG10(hwInfo) && revision < REVISION_B) {
                EXPECT_EQ(aub_stream::ENGINE_RCS, hwInfo.capabilityTable.defaultEngineType);
            } else {
                EXPECT_EQ(aub_stream::ENGINE_CCS, defaultHwInfo->capabilityTable.defaultEngineType);
            }
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G11OrG12WhenAskingIfMaxThreadsForWorkgroupWAIsRequiredThenReturnFalse) {
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        for (auto deviceId : {dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
            hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.platform.usDeviceID = deviceId;

            EXPECT_FALSE(productHelper->isMaxThreadsForWorkgroupWARequired(hwInfo));
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10A0OrA1SteppingWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto hwInfo = *defaultHwInfo;
    std::vector<std::pair<unsigned short, uint16_t>> dg2Configs =
        {{dg2G10DeviceIds[0], revIdA0},
         {dg2G10DeviceIds[0], revIdA1},
         {dg2G10DeviceIds[0], revIdB0},
         {dg2G10DeviceIds[0], revIdC0},
         {dg2G11DeviceIds[0], revIdA0},
         {dg2G11DeviceIds[0], revIdB0},
         {dg2G11DeviceIds[0], revIdB1},
         {dg2G12DeviceIds[0], revIdA0}};

    for (const auto &[deviceID, revisionID] : dg2Configs) {
        hwInfo.platform.usRevId = revisionID;
        hwInfo.platform.usDeviceID = deviceID;
        hwInfo.ipVersion.value = compilerProductHelper->getHwIpVersion(hwInfo);
        auto expectedValue = DG2::isG10(hwInfo) && revisionID < revIdB0;
        refreshReleaseHelper(&hwInfo);

        EXPECT_EQ(expectedValue, productHelper->isDefaultEngineTypeAdjustmentRequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10WhenAskingForSBAWaThenReturnSuccessOnlyForBStepping) {
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];

        auto expectedValue = revision == REVISION_B;
        EXPECT_EQ(expectedValue, productHelper->isAdditionalStateBaseAddressWARequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G11WhenAskingForSBAWaThenReturnSuccess) {
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G11DeviceIds[0];

        EXPECT_TRUE(productHelper->isAdditionalStateBaseAddressWARequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G12WhenAskingForSBAWaThenReturnSuccess) {
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G12DeviceIds[0];

        EXPECT_FALSE(productHelper->isAdditionalStateBaseAddressWARequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenInvalidArchitectureInIpVersionWhenRefreshReleaseHelperThenNullptrIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.ipVersion.value = AOT::DG2_G10_A0;
    hwInfo.ipVersion.architecture = 0;

    refreshReleaseHelper(&hwInfo);
    EXPECT_EQ(releaseHelper, nullptr);
}

DG2TEST_F(ProductHelperTestDg2, givenInvalidReleaseInIpVersionWhenRefreshReleaseHelperThenNullptrIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.ipVersion.value = AOT::DG2_G10_A0;
    hwInfo.ipVersion.release = 0;

    refreshReleaseHelper(&hwInfo);
    EXPECT_EQ(releaseHelper, nullptr);
}

DG2TEST_F(ProductHelperTestDg2, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);
    auto hwInfo = *defaultHwInfo;

    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = false;

        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_TRUE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    auto hwInfo = *defaultHwInfo;
    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = true;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_TRUE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProgramPipeControlPriorToNonPipelinedStateCommandDisabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    auto hwInfo = *defaultHwInfo;
    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = true;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = false;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_TRUE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = true;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithSingleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = false;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithSingleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
        hwInfo.platform.usDeviceID = deviceId;
        auto isRcs = true;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenNonG10G11OrG12Dg2WhenProductHelperWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenNoWaIsNeededUnlessForcedByDebugFlag) {

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = 0;
    refreshReleaseHelper(&hwInfo);
    EXPECT_FALSE(DG2::isG10(hwInfo));
    EXPECT_FALSE(DG2::isG11(hwInfo));
    EXPECT_FALSE(DG2::isG12(hwInfo));

    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = false;

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);
        EXPECT_TRUE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2WhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {

    EXPECT_TRUE(productHelper->isBlitterForImagesSupported());
}

DG2TEST_F(ProductHelperTestDg2, WhenGetSvmCpuAlignmentThenProperValueIsReturned) {

    EXPECT_EQ(MemoryConstants::pageSize2M, productHelper->getSvmCpuAlignment());
}

DG2TEST_F(ProductHelperTestDg2, givenB0rCSteppingWhenAskingIfTile64With3DSurfaceOnBCSIsSupportedThenReturnTrue) {

    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, false},
         {REVISION_A1, false},
         {REVISION_B, false},
         {REVISION_C, true}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);

        productHelper->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper->isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10WhenAskingForTile64For3dSurfaceOnBcsSupportThenReturnSuccessOnlyForCStepping) {

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];

        auto expectedValue = revision == REVISION_C;

        EXPECT_EQ(expectedValue, productHelper->isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G11WhenAskingForTile64For3dSurfaceOnBcsSupportThenReturnSuccessOnlyForHigherThanAStepping) {

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G11DeviceIds[0];

        auto expectedValue = revision >= REVISION_B;

        EXPECT_EQ(expectedValue, productHelper->isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G12WhenAskingForTile64For3dSurfaceOnBcsSupportThenReturnSuccess) {

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G12DeviceIds[0];

        EXPECT_TRUE(productHelper->isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    auto hardwareInfo = *defaultHwInfo;

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping <= REVISION_B) {
            if (stepping == REVISION_A0) {
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, *productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, *productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, *productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, *productHelper));
            } else if (stepping == REVISION_A1) {
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, *productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, *productHelper));
            } else { // REVISION_B
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, *productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, *productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, *productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, *productHelper));
            }
        } else {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, *productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, *productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, *productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, *productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, *productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, *productHelper));
        }

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo, *productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, *productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo, *productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A1, hardwareInfo, *productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_A1, hardwareInfo, *productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo, *productHelper));

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, *productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo, *productHelper));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenRevisionEnumAndDisableL3CacheForDebugCalledThenCorrectValueIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.platform.usDeviceID = dg2G10DeviceIds[0];

    for (auto &stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(stepping, hardwareInfo);
        if (stepping < REVISION_B) {
            EXPECT_TRUE(productHelper->disableL3CacheForDebug(hardwareInfo));
        } else {
            EXPECT_FALSE(productHelper->disableL3CacheForDebug(hardwareInfo));
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2WhenSetForceNonCoherentThenProperFlagSet) {
    using FORCE_NON_COHERENT = typename FamilyType::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    auto stateComputeMode = FamilyType::cmdInitStateComputeMode;
    auto properties = StateComputeModeProperties{};

    properties.isCoherencyRequired.set(false);
    productHelper->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(XeHpgCoreFamily::stateComputeModeForceNonCoherentMask, stateComputeMode.getMaskBits());

    properties.isCoherencyRequired.set(true);
    productHelper->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(XeHpgCoreFamily::stateComputeModeForceNonCoherentMask, stateComputeMode.getMaskBits());
}

DG2TEST_F(ProductHelperTestDg2, givenEnabledSliceInNonStandardConfigWhenComputeUnitsUsedForScratchThenProperCalculationIsReturned) {
    HardwareInfo &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    testSysInfo.MaxSlicesSupported = 2;
    testSysInfo.SliceCount = 2;
    testSysInfo.IsDynamicallyPopulated = true;
    for (auto &sliceInfo : testSysInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    testSysInfo.SliceInfo[2].Enabled = true;
    testSysInfo.SliceInfo[3].Enabled = true;
    auto highestEnabledSlice = 4;
    auto subSlicesPerSlice = testSysInfo.MaxSubSlicesSupported / testSysInfo.MaxSlicesSupported;
    auto maxSubSlice = highestEnabledSlice * subSlicesPerSlice;

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    uint32_t expectedCalculation = maxSubSlice * testSysInfo.MaxEuPerSubSlice * (testSysInfo.ThreadCount / testSysInfo.EUCount);

    EXPECT_EQ(expectedCalculation, gfxCoreHelper.getComputeUnitsUsedForScratch(*executionEnvironment->rootDeviceEnvironments[0]));
}

DG2TEST_F(ProductHelperTestDg2, givenNotEnabledSliceWhenComputeUnitsUsedForScratchThenThrowUnrecoverableIf) {
    HardwareInfo &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    testSysInfo.IsDynamicallyPopulated = false;
    testSysInfo.MaxSlicesSupported = 0;

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    EXPECT_THROW(gfxCoreHelper.getComputeUnitsUsedForScratch(*executionEnvironment->rootDeviceEnvironments[0]), std::exception);
}

DG2TEST_F(ProductHelperTestDg2, givenDG2WhenCheckingIsTimestampWaitSupportedForEventsThenReturnTrue) {

    EXPECT_TRUE(productHelper->isTimestampWaitSupportedForEvents());
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenCallGetCommandBuffersPreallocatedPerCommandQueueThenReturnCorrectValue) {
    EXPECT_EQ(2u, productHelper->getCommandBuffersPreallocatedPerCommandQueue());
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue) {
    EXPECT_EQ(productHelper->getInternalHeapsPreallocated(), 1u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper->getInternalHeapsPreallocated(), 3u);
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

DG2TEST_F(ProductConfigTests, givenInvalidRevisionIdWhenDeviceIdIsDefaultThenDefaultConfigIsReturned) {
    hwInfo.platform.usDeviceID = 0;
    hwInfo.platform.usRevId = CommonConstants::invalidRevisionID;

    productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(productConfig, AOT::DG2_G10_C0);
}

DG2TEST_F(ProductConfigTests, givenDg2G10DeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : dg2G10DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_A0);

        hwInfo.platform.usRevId = 0x1;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_A1);

        hwInfo.platform.usRevId = 0x4;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_B0);

        hwInfo.platform.usRevId = 0x8;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_C0);
    }
}

DG2TEST_F(ProductConfigTests, givenDg2DeviceIdWhenIncorrectRevisionIsPassedThenDefaultConfigIsReturned) {
    for (const auto *dg2 : {&dg2G10DeviceIds, &dg2G11DeviceIds}) {
        for (const auto &deviceId : *dg2) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = CommonConstants::invalidRevisionID;
            productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
            EXPECT_EQ(productConfig, AOT::DG2_G10_C0);
        }
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G11DeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : dg2G11DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_A0);

        hwInfo.platform.usRevId = 0x4;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_B0);

        hwInfo.platform.usRevId = 0x5;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_B1);
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G12DeviceIdWhenGetProductConfigThenCorrectConfigIsReturned) {
    for (const auto &deviceId : dg2G12DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G12_A0);
    }
}

DG2TEST_F(ProductConfigTests, givenNotSetDeviceAndRevisionIdWhenGetProductConfigThenDefaultConfigIsReturned) {
    hwInfo.platform.usRevId = 0x0;
    hwInfo.platform.usDeviceID = 0x0;

    productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(productConfig, AOT::DG2_G10_C0);
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenAskedIfStorageInfoAdjustmentIsRequiredThenTrueIsReturned) {
    if constexpr (is32bit) {
        EXPECT_TRUE(productHelper->isStorageInfoAdjustmentRequired());
    } else {
        EXPECT_FALSE(productHelper->isStorageInfoAdjustmentRequired());
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenGettingUseLocalPreferredForCacheableBuffersThenExpectTrue) {
    EXPECT_TRUE(productHelper->useLocalPreferredForCacheableBuffers());
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenCheckingIsHostDeviceUsmPoolAllocatorSupportedThenCorrectValueIsReturned) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_TRUE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_FALSE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
}
