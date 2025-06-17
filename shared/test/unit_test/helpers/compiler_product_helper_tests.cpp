/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CompilerProductHelperFixture = Test<DeviceFixture>;

HWTEST_F(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.flags.ftrWalkerMTP = false;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_FALSE(compilerProductHelper.isMidThreadPreemptionSupported(hwInfo));
    hwInfo.featureTable.flags.ftrWalkerMTP = true;
    EXPECT_TRUE(compilerProductHelper.isMidThreadPreemptionSupported(hwInfo));
}

TEST(CompilerProductHelperTest, GivenIgcLibraryNameDebugKeyWhenQueryingForCustomIgcLibraryNameThenDebugKeyValueisReturned) {
    DebugManagerStateRestore restorer;

    debugManager.flags.IgcLibraryName.set("my_custom_igc.so");
    auto compilerProductHelper = CompilerProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_STREQ("my_custom_igc.so", compilerProductHelper->getCustomIgcLibraryName());
}

TEST(CompilerProductHelperTest, WhenCompilerProductHelperCreateIsCalledWithUnknownProductThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, CompilerProductHelper::create(IGFX_UNKNOWN));
}

HWTEST2_F(CompilerProductHelperFixture, GivenProductBeforeXeHpcWhenIsForceToStatelessRequiredThenFalseIsReturned, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = getHelper<CompilerProductHelper>();
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterWhenIsForceToStatelessRequiredThenCorrectResultIsReturned, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    debugManager.flags.DisableForceToStateless.set(false);
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    debugManager.flags.DisableForceToStateless.set(true);
    if constexpr (FamilyType::isHeaplessRequired()) {
        EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());
    } else {
        EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenGen11AndLaterThenSubgroupLocalBlockIoIsSupported, MatchAny) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupLocalBlockIoSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenDotAccumulateIsSupported, IsAtLeastXeCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isDotAccumulateSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenDotAccumulateIsNotSupported, IsGen12LP) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isDotAccumulateSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenCreateBufferWithPropertiesIsSupported, IsAtLeastXeCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isCreateBufferWithPropertiesSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenCreateBufferWithPropertiesIsNotSupported, IsGen12LP) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isCreateBufferWithPropertiesSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupNamedBarrierIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupNamedBarrierSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupNamedBarrierIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupNamedBarrierSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupExtendedBlockReadIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupExtendedBlockReadSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupExtendedBlockReadIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupExtendedBlockReadSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroup2DBlockIOIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroup2DBlockIOSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroup2DBlockIOIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroup2DBlockIOSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupBufferPrefetchIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupBufferPrefetchSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupBufferPrefetchIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupBufferPrefetchSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenCompilerProductHelperThenBFloat16ConversionIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isBFloat16ConversionSupported(), compilerProductHelper.isBFloat16ConversionSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isBFloat16ConversionSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenBFloat16ConversionIsSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();
    EXPECT_TRUE(compilerProductHelper.isBFloat16ConversionSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenMatrixMultiplyAccumulateIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isMatrixMultiplyAccumulateSupported(), compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenMatrixMultiplyAccumulateTF32IsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto hwInfo = *defaultHwInfo;

    EXPECT_TRUE(compilerProductHelper.isMatrixMultiplyAccumulateTF32Supported(hwInfo));
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenMatrixMultiplyAccumulateTF32IsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto hwInfo = *defaultHwInfo;

    EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateTF32Supported(hwInfo));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenDotProductAccumulateSystolicIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isDotProductAccumulateSystolicSupported(), compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenMatrixMultiplyAccumulateIsSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_TRUE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenDotProductAccumulateSystolicIsSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_TRUE(compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenSplitMatrixMultiplyAccumulateIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isSplitMatrixMultiplyAccumulateSupported(), compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenSplitMatrixMultiplyAccumulateIsNotSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_FALSE(compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenBindlessAddressingIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isBindlessAddressingDisabled(), compilerProductHelper.isBindlessAddressingDisabled(releaseHelper));
    } else {
        EXPECT_TRUE(compilerProductHelper.isBindlessAddressingDisabled(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenBindlessAddressingIsNotSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_TRUE(compilerProductHelper.isBindlessAddressingDisabled(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, givenAotConfigWhenSetHwInfoRevisionIdThenCorrectValueIsSet, IsAtMostDg2) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto hwInfo = *defaultHwInfo;
    auto productConfig = compilerProductHelper.getHwIpVersion(*defaultHwInfo);
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    EXPECT_EQ(hwInfo.platform.usRevId, aotConfig.revision);
    EXPECT_EQ(hwInfo.ipVersion.value, aotConfig.value);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtMostXeHPWhenGetCachingPolicyOptionsThenReturnNullptr, IsGen12LP) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_EQ(compilerProductHelper.getCachingPolicyOptions(false), nullptr);
    EXPECT_EQ(compilerProductHelper.getCachingPolicyOptions(true), nullptr);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption, IsAtLeastXeCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteBackPolicyOption, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreAndDebugFlagSetForceAllResourcesUncachedWhenGetCachingPolicyOptionsThenReturnUncachedPolicyOption, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    debugManager.flags.ForceAllResourcesUncached.set(true);

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=2";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenCachePolicyWithoutCorrespondingBuildOptionWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(5);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_EQ(nullptr, compilerProductHelper.getCachingPolicyOptions(false));
    EXPECT_EQ(nullptr, compilerProductHelper.getCachingPolicyOptions(true));
}

TEST_F(CompilerProductHelperFixture, givenHwInfoWithIndependentForwardProgressThenReportsClKhrSubgroupExtension) {

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsIndependentForwardProgress = true;
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_khr_subgroups")));

    hwInfo.capabilityTable.supportsIndependentForwardProgress = false;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_subgroups")));
}

TEST_F(CompilerProductHelperFixture, givenHwInfoWithCLVersionAtLeast20ThenReportsClExtFloatAtomicsExtension) {

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.clVersionSupport = 20;
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));

    hwInfo.capabilityTable.clVersionSupport = 21;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));

    hwInfo.capabilityTable.clVersionSupport = 30;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));

    hwInfo.capabilityTable.clVersionSupport = 12;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));
}

TEST_F(CompilerProductHelperFixture, givenHwInfoWithCLVersion30ThenReportsClKhrExternalMemoryExtension) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.clVersionSupport = 30;
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_khr_external_memory")));

    hwInfo.capabilityTable.clVersionSupport = 21;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_external_memory")));

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ClKhrExternalMemoryExtension.set(0);

    hwInfo.capabilityTable.clVersionSupport = 30;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_external_memory")));
}

HWTEST2_F(CompilerProductHelperFixture, GivenAtLeastGen12lpDeviceWhenCheckingIfIntegerDotExtensionIsSupportedThenTrueReturned, MatchAny) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isDotIntegerProductExtensionSupported());
}

HWTEST2_F(CompilerProductHelperFixture, givenConfigWhenMatchConfigWithRevIdThenProperConfigIsReturned, IsNotPvcOrDg2) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto config = hwInfo.ipVersion.value;
    for (uint32_t rev : {0x0, 0x1, 0x4}) {
        NEO::HardwareIpVersion matchedIpVersion;
        matchedIpVersion.value = compilerProductHelper.matchRevisionIdWithProductConfig(config, rev);
        EXPECT_EQ(hwInfo.ipVersion.architecture, matchedIpVersion.architecture) << rev;
        EXPECT_EQ(hwInfo.ipVersion.release, matchedIpVersion.release) << rev;
        bool gotSameRevision = (matchedIpVersion.revision == hwInfo.ipVersion.revision);
        bool gotRequestedRevision = (matchedIpVersion.revision == rev);
        EXPECT_TRUE(gotSameRevision || gotRequestedRevision) << rev;
    }
}

HWTEST_F(CompilerProductHelperFixture, givenProductHelperWhenGetAndOverrideHwIpVersionThenCorrectMatchIsFound) {
    DebugManagerStateRestore stateRestore;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    uint32_t config = 0x1234;
    debugManager.flags.OverrideHwIpVersion.set(config);
    hwInfo.ipVersion.value = 0x5678;
    EXPECT_EQ(compilerProductHelper.getHwIpVersion(hwInfo), config);
}

HWTEST2_F(CompilerProductHelperFixture, givenCompilerProductHelperWhenIsHeaplessModeEnabledThenFalseIsReturned, IsAtMostXe3Core) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_FALSE(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo));
}

HWTEST_F(CompilerProductHelperFixture, WhenFullListOfSupportedOpenCLCVersionsIsRequestedThenReturnsListOfAllSupportedVersionsByTheAssociatedDevice) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{3, 0});
    ASSERT_LT(3U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);

    EXPECT_EQ(1, versions[2].major);
    EXPECT_EQ(2, versions[2].minor);

    if (pDevice->getHardwareInfo().capabilityTable.clVersionSupport == 30) {
        ASSERT_EQ(4U, versions.size());
        EXPECT_EQ(3, versions[3].major);
        EXPECT_EQ(0, versions[3].minor);
    } else {
        EXPECT_EQ(3U, versions.size());
    }
}

HWTEST_F(CompilerProductHelperFixture, WhenLimitedListOfSupportedOpenCLCVersionsIsRequestedThenReturnsListOfAllSupportedVersionsByTheAssociatedDeviceTrimmedToProvidedMax) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{1, 1});
    ASSERT_EQ(2U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForLimitedListOfSupportedOpenCLCVersionsWhenMaxVersionIsEmptyThenReturnsListOfAllSupportedVersionsByTheAssociatedDevice) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{0, 0});
    ASSERT_LT(3U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);

    EXPECT_EQ(1, versions[2].major);
    EXPECT_EQ(2, versions[2].minor);

    if (pDevice->getHardwareInfo().capabilityTable.clVersionSupport == 30) {
        ASSERT_EQ(4U, versions.size());
        EXPECT_EQ(3, versions[3].major);
        EXPECT_EQ(0, versions[3].minor);
    } else {
        EXPECT_EQ(3U, versions.size());
    }
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForLimitedListOfSupportedOpenCLCVersionsWhenMaxVersionIsBelow10ThenReturnsListOfAllSupportedVersionsByTheAssociatedDeviceTrimmedToOclC12) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{0, 1});
    ASSERT_EQ(3U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);

    EXPECT_EQ(1, versions[2].major);
    EXPECT_EQ(2, versions[2].minor);
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForKernelFp16CapabilitiesThenReturnMinMaxAndLoadStoreCapabilitiesWithCapsFromReleaseHelperIfPresent) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();

    uint32_t fp16Caps = 0u;
    compilerProductHelper.getKernelFp16AtomicCapabilities(releaseHelper, fp16Caps);
    EXPECT_TRUE(isValueSet(fp16Caps, FpAtomicExtFlags::minMaxAtomicCaps));
    EXPECT_TRUE(isValueSet(fp16Caps, FpAtomicExtFlags::loadStoreAtomicCaps));

    if (releaseHelper) {
        uint32_t extraFp16Caps = releaseHelper->getAdditionalFp16Caps();
        if (0u != extraFp16Caps) {
            EXPECT_TRUE(isValueSet(fp16Caps, extraFp16Caps));
        }
    }
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForKernelFp32CapabilitiesThenReturnAddMinMaxAndLoadStoreCapabilities) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    uint32_t fp32Caps = 0u;
    compilerProductHelper.getKernelFp32AtomicCapabilities(fp32Caps);
    EXPECT_TRUE(isValueSet(fp32Caps, FpAtomicExtFlags::addAtomicCaps));
    EXPECT_TRUE(isValueSet(fp32Caps, FpAtomicExtFlags::minMaxAtomicCaps));
    EXPECT_TRUE(isValueSet(fp32Caps, FpAtomicExtFlags::loadStoreAtomicCaps));
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForKernelFp64CapabilitiesThenReturnAddMinMaxAndLoadStoreCapabilities) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    uint32_t fp64Caps = 0u;
    compilerProductHelper.getKernelFp64AtomicCapabilities(fp64Caps);
    EXPECT_TRUE(isValueSet(fp64Caps, FpAtomicExtFlags::addAtomicCaps));
    EXPECT_TRUE(isValueSet(fp64Caps, FpAtomicExtFlags::minMaxAtomicCaps));
    EXPECT_TRUE(isValueSet(fp64Caps, FpAtomicExtFlags::loadStoreAtomicCaps));
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForExtraKernelCapabilitiesThenReturnCapabilitiesFromReleaseHelperOrNoneIfReleaseHelperIsNotPresent) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();

    uint32_t extraCaps = 0u;
    compilerProductHelper.getKernelCapabilitiesExtra(releaseHelper, extraCaps);
    if (releaseHelper) {
        uint32_t extraCapsFromRH = releaseHelper->getAdditionalExtraCaps();
        if (0u != extraCapsFromRH) {
            EXPECT_TRUE(isValueSet(extraCaps, extraCapsFromRH));
        } else {
            EXPECT_EQ(0u, extraCaps);
        }
    } else {
        EXPECT_EQ(0u, extraCaps);
    }
}
