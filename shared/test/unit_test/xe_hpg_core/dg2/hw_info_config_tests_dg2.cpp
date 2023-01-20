/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe_hpg_core/dg2/product_configs_dg2.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "aubstream/product_family.h"

using namespace NEO;

using ProductHelperTestDg2 = Test<DeviceFixture>;
using ProductHelperTestDg2 = Test<DeviceFixture>;

using Dg2HwInfo = ProductHelperTest;

DG2TEST_F(Dg2HwInfo, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Dg2, productHelper->getAubStreamProductFamily());
}

DG2TEST_F(ProductHelperTestDg2, givenDg2ConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
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

DG2TEST_F(ProductHelperTestDg2, givenDg2ConfigWhenSetupHardwareInfoThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    Dg2HwConfig::setupHardwareInfo(&hwInfo, false);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
}

DG2TEST_F(ProductHelperTestDg2, givenG10DevIdWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    auto &productHelper = *ProductHelper::get(defaultHwInfo->platform.eProductFamily);
    HardwareInfo myHwInfo = *defaultHwInfo;
    myHwInfo.platform.usDeviceID = dg2G10DeviceIds[0];
    EXPECT_FALSE(productHelper.isDisableOverdispatchAvailable(myHwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_FALSE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));

    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

DG2TEST_F(ProductHelperTestDg2, givenG11DevIdWhenIsDisableOverdispatchAvailableCalledThenTrueReturnedForAllSteppings) {
    FrontEndPropertiesSupport fePropertiesSupport{};
    auto &productHelper = *ProductHelper::get(defaultHwInfo->platform.eProductFamily);
    HardwareInfo myHwInfo = *defaultHwInfo;

    myHwInfo.platform.usDeviceID = dg2G11DeviceIds[0];
    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_C, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

DG2TEST_F(ProductHelperTestDg2, givenG12DevIdWhenIsDisableOverdispatchAvailableCalledThenTrueReturnedForAllSteppings) {
    FrontEndPropertiesSupport fePropertiesSupport{};
    auto &productHelper = *ProductHelper::get(defaultHwInfo->platform.eProductFamily);
    HardwareInfo myHwInfo = *defaultHwInfo;

    myHwInfo.platform.usDeviceID = dg2G12DeviceIds[0];
    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    myHwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_C, myHwInfo);
    EXPECT_TRUE(productHelper.isDisableOverdispatchAvailable(myHwInfo));
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, myHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

DG2TEST_F(ProductHelperTestDg2, whenAdjustingDefaultEngineTypeThenSelectEngineTypeBasedOnRevisionId) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.capabilityTable.defaultEngineType = defaultHwInfo->capabilityTable.defaultEngineType;
            gfxCoreHelper.adjustDefaultEngineType(&hwInfo);
            if (DG2::isG10(hwInfo) && revision < REVISION_B) {
                EXPECT_EQ(aub_stream::ENGINE_RCS, hwInfo.capabilityTable.defaultEngineType);
            } else {
                EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
            }
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G11OrG12WhenAskingIfMaxThreadsForWorkgroupWAIsRequiredThenReturnFalse) {
    auto &productHelper = getHelper<ProductHelper>();
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        for (auto deviceId : {dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.platform.usDeviceID = deviceId;

            EXPECT_FALSE(productHelper.isMaxThreadsForWorkgroupWARequired(hwInfo));
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10A0OrA1SteppingWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.platform.usDeviceID = deviceId;

            auto expectedValue = DG2::isG10(hwInfo) && revision < REVISION_B;

            EXPECT_EQ(expectedValue, productHelper.isDefaultEngineTypeAdjustmentRequired(hwInfo));
            EXPECT_EQ(expectedValue, productHelper.isAllocationSizeAdjustmentRequired(hwInfo));
            EXPECT_EQ(expectedValue, productHelper.isPrefetchDisablingRequired(hwInfo));
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10WhenAskingForSBAWaThenReturnSuccessOnlyForBStepping) {
    auto &productHelper = getHelper<ProductHelper>();
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];

        auto expectedValue = revision == REVISION_B;
        EXPECT_EQ(expectedValue, productHelper.isAdditionalStateBaseAddressWARequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G11WhenAskingForSBAWaThenReturnSuccess) {
    auto &productHelper = getHelper<ProductHelper>();
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G11DeviceIds[0];

        EXPECT_TRUE(productHelper.isAdditionalStateBaseAddressWARequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G12WhenAskingForSBAWaThenReturnSuccess) {
    auto &productHelper = getHelper<ProductHelper>();
    auto hwInfo = *defaultHwInfo;
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G12DeviceIds[0];

        EXPECT_FALSE(productHelper.isAdditionalStateBaseAddressWARequired(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenProgramPipeControlPriorToNonPipelinedStateCommandDisabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_TRUE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithSingleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto isRcs = false;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWithSingleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto isRcs = true;

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

DG2TEST_F(ProductHelperTestDg2, givenDg2WhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    EXPECT_TRUE(productHelper.isBlitterForImagesSupported());
}

DG2TEST_F(ProductHelperTestDg2, WhenGetSvmCpuAlignmentThenProperValueIsReturned) {
    const auto &productHelper = *ProductHelper::get(productFamily);
    EXPECT_EQ(MemoryConstants::pageSize2Mb, productHelper.getSvmCpuAlignment());
}

DG2TEST_F(ProductHelperTestDg2, givenB0rCSteppingWhenAskingIfTile64With3DSurfaceOnBCSIsSupportedThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, false},
         {REVISION_A1, false},
         {REVISION_B, false},
         {REVISION_C, true}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10A0WhenConfigureCalledThenDisableCompression) {
    auto &productHelper = getHelper<ProductHelper>();

    for (uint8_t revision : {REVISION_A0, REVISION_A1}) {
        for (auto deviceId : {dg2G10DeviceIds[0], dg2G11DeviceIds[0], dg2G12DeviceIds[0]}) {
            HardwareInfo hwInfo = *defaultHwInfo;
            hwInfo.featureTable.flags.ftrE2ECompression = true;

            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            hwInfo.platform.usDeviceID = deviceId;

            productHelper.configureHardwareCustom(&hwInfo, nullptr);

            auto compressionExpected = DG2::isG10(hwInfo) ? (revision != REVISION_A0) : true;

            EXPECT_EQ(compressionExpected, hwInfo.capabilityTable.ftrRenderCompressedBuffers);
            EXPECT_EQ(compressionExpected, hwInfo.capabilityTable.ftrRenderCompressedImages);
            EXPECT_EQ(compressionExpected, productHelper.allowCompression(hwInfo));
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G10WhenAskingForTile64For3dSurfaceOnBcsSupportThenReturnSuccessOnlyForCStepping) {
    auto &productHelper = getHelper<ProductHelper>();

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];

        auto expectedValue = revision == REVISION_C;

        EXPECT_EQ(expectedValue, productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G11WhenAskingForTile64For3dSurfaceOnBcsSupportThenReturnSuccessOnlyForHigherThanAStepping) {
    auto &productHelper = getHelper<ProductHelper>();

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G11DeviceIds[0];

        auto expectedValue = revision >= REVISION_B;

        EXPECT_EQ(expectedValue, productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2G12WhenAskingForTile64For3dSurfaceOnBcsSupportThenReturnSuccess) {
    auto &productHelper = getHelper<ProductHelper>();

    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usDeviceID = dg2G12DeviceIds[0];

        EXPECT_TRUE(productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo));
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
    const auto &productHelper = getHelper<ProductHelper>();

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping <= REVISION_B) {
            if (stepping == REVISION_A0) {
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
            } else if (stepping == REVISION_A1) {
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
            } else { // REVISION_B
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
            }
        } else {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
        }

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A1, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_A1, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo, productHelper));

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo, productHelper));
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
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);
        if (stepping < REVISION_B) {
            EXPECT_TRUE(gfxCoreHelper.disableL3CacheForDebug(hardwareInfo, productHelper));
        } else {
            EXPECT_FALSE(gfxCoreHelper.disableL3CacheForDebug(hardwareInfo, productHelper));
        }
    }
}

DG2TEST_F(ProductHelperTestDg2, givenDg2WhenSetForceNonCoherentThenProperFlagSet) {
    using FORCE_NON_COHERENT = typename FamilyType::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    auto productHelper = ProductHelper::get(productFamily);

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
    HardwareInfo &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    testSysInfo.IsDynamicallyPopulated = true;
    for (int i = 0; i < GT_MAX_SLICE; i++) {
        testSysInfo.SliceInfo[i].Enabled = false;
    }
    testSysInfo.SliceInfo[2].Enabled = true;
    testSysInfo.SliceInfo[3].Enabled = true;
    auto highestEnabledSlice = 4;
    auto subSlicesPerSlice = testSysInfo.MaxSubSlicesSupported / testSysInfo.MaxSlicesSupported;
    auto maxSubSlice = highestEnabledSlice * subSlicesPerSlice;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    uint32_t expectedCalculation = maxSubSlice * testSysInfo.MaxEuPerSubSlice * (testSysInfo.ThreadCount / testSysInfo.EUCount);

    EXPECT_EQ(expectedCalculation, gfxCoreHelper.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment()));
}

DG2TEST_F(ProductHelperTestDg2, givenNotEnabledSliceWhenComputeUnitsUsedForScratchThenThrowUnrecoverableIf) {
    HardwareInfo &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    testSysInfo.IsDynamicallyPopulated = true;
    for (int i = 0; i < GT_MAX_SLICE; i++) {
        testSysInfo.SliceInfo[i].Enabled = false;
    }

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_THROW(gfxCoreHelper.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment()), std::exception);
}

DG2TEST_F(ProductHelperTestDg2, givenDG2WhenCheckingIsTimestampWaitSupportedForEventsThenReturnTrue) {
    auto &helper = getHelper<ProductHelper>();
    EXPECT_TRUE(helper.isTimestampWaitSupportedForEvents());
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

    productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
}

DG2TEST_F(ProductConfigTests, givenDg2G10DeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : dg2G10DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_A0);

        hwInfo.platform.usRevId = 0x1;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_A1);

        hwInfo.platform.usRevId = 0x4;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_B0);

        hwInfo.platform.usRevId = 0x8;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G10_C0);
    }
}

DG2TEST_F(ProductConfigTests, givenDg2DeviceIdWhenIncorrectRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto *dg2 : {&dg2G10DeviceIds, &dg2G11DeviceIds}) {
        for (const auto &deviceId : *dg2) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = CommonConstants::invalidRevisionID;
            productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
            EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
        }
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G11DeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : dg2G11DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_A0);

        hwInfo.platform.usRevId = 0x4;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_B0);

        hwInfo.platform.usRevId = 0x5;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G11_B1);
    }
}

DG2TEST_F(ProductConfigTests, givenDg2G12DeviceIdWhenGetProductConfigThenCorrectConfigIsReturned) {
    for (const auto &deviceId : dg2G12DeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::DG2_G12_A0);
    }
}

DG2TEST_F(ProductConfigTests, givenNotSetDeviceAndRevisionIdWhenGetProductConfigThenUnknownIsaIsReturned) {
    hwInfo.platform.usRevId = 0x0;
    hwInfo.platform.usDeviceID = 0x0;

    productConfig = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenAskedIfStorageInfoAdjustmentIsRequiredThenTrueIsReturned) {
    auto productHelper = ProductHelper::get(defaultHwInfo->platform.eProductFamily);
    if constexpr (is32bit) {
        EXPECT_TRUE(productHelper->isStorageInfoAdjustmentRequired());
    } else {
        EXPECT_FALSE(productHelper->isStorageInfoAdjustmentRequired());
    }
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(productHelper.isEvictionIfNecessaryFlagSupported());
}

DG2TEST_F(ProductHelperTestDg2, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    auto productHelper = ProductHelper::get(defaultHwInfo->platform.eProductFamily);

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(*defaultHwInfo, false));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(*defaultHwInfo, true));

    DebugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(*defaultHwInfo, false));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(*defaultHwInfo, true));

    DebugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(*defaultHwInfo, false));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(*defaultHwInfo, true));
}

DG2TEST_F(ProductHelperTestDg2, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    auto productHelper = ProductHelper::get(defaultHwInfo->platform.eProductFamily);

    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}
