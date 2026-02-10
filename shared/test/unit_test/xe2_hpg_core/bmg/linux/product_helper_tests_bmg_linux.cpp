/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

struct BmgProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        drm = new DrmMockExtended(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }
};

BMGTEST_F(BmgProductHelperLinux, WhenConfiguringHwInfoThenZeroIsReturned) {

    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
}

BMGTEST_F(BmgProductHelperLinux, given57bAddressSpaceWhenConfiguringHwInfoThenSetFtrFlag) {

    outHwInfo.featureTable.flags.ftr57bGPUAddressing = false;
    outHwInfo.platform.eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48);

    int ret = productHelper->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.featureTable.flags.ftr57bGPUAddressing);

    outHwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(57);
    ret = productHelper->configureHardwareCustom(&outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.featureTable.flags.ftr57bGPUAddressing);
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isVmBindPatIndexProgrammingSupported());
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenAskedIfIsTlbFlushRequiredThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->isTlbFlushRequired());
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenAskedIsPageFaultSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenAskedIsKmdMigrationSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isKmdMigrationSupported());
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenAskedGetSharedSystemPatIndexThenReturnCorrectValue) {
    EXPECT_EQ(0ull, productHelper->getSharedSystemPatIndex());
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenAskedUseSharedSystemUsmThenReturnCorrectValue) {
    EXPECT_FALSE(productHelper->useSharedSystemUsm());
}

BMGTEST_F(BmgProductHelperLinux, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &BmgHwConfig::hwInfo, &BmgHwConfig::setupHardwareInfo};
    drm.overrideDeviceDescriptor = &device;

    int ret = drm.setupHardwareInfo(0, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 0u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}

BMGTEST_F(BmgProductHelperLinux, givenProductHelperWhenCallDeferMOCSToPatOnWSLThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.deferMOCSToPatIndex(true));
}

BMGTEST_F(BmgProductHelperLinux, givenOsInterfaceIsNullWhenGetDeviceMemoryMaxClkRateIsCalledThenReturnZero) {
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxClkRate(pInHwInfo, nullptr, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenMockDriverModelWithUnknownTypeWhenGetDeviceMemoryMaxClkRateIsCalledThenReturnZero) {
    // This simulates the test scenario that was causing the abort
    auto mockDriverModel = std::make_unique<MockDriverModel>(); // Defaults to DriverModelType::unknown
    osInterface->setDriverModel(std::move(mockDriverModel));
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxClkRate(pInHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenDrmQueryFailsWhenGetDeviceMemoryMaxClkRateIsCalledThenReturnZero) {
    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = false;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxClkRate(pInHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenDrmQuerySucceedsWhenGetDeviceMemoryMaxClkRateIsCalledThenReturnClockRate) {
    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;
    EXPECT_EQ(800u, productHelper->getDeviceMemoryMaxClkRate(pInHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenOsInterfaceIsNullWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnZero) {
    EXPECT_EQ(0u, productHelper->getDeviceMemoryPhysicalSizeInBytes(nullptr, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenMockDriverModelWithUnknownTypeWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnZero) {
    auto mockDriverModel = std::make_unique<MockDriverModel>();
    osInterface->setDriverModel(std::move(mockDriverModel));
    EXPECT_EQ(0u, productHelper->getDeviceMemoryPhysicalSizeInBytes(osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenDrmQueryFailsWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnZero) {
    drm->storedGetDeviceMemoryPhysicalSizeInBytesStatus = false;
    drm->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
    EXPECT_EQ(0u, productHelper->getDeviceMemoryPhysicalSizeInBytes(osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenDrmQuerySucceedsWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenReturnPhysicalSize) {
    drm->storedGetDeviceMemoryPhysicalSizeInBytesStatus = true;
    drm->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
    EXPECT_EQ(1024u, productHelper->getDeviceMemoryPhysicalSizeInBytes(osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenOsInterfaceIsNullWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnZero) {
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(pInHwInfo, nullptr, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenMockDriverModelWithUnknownTypeWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnZero) {
    auto mockDriverModel = std::make_unique<MockDriverModel>();
    osInterface->setDriverModel(std::move(mockDriverModel));
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(pInHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenDrmClockRateQueryFailsWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturnZero) {
    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = false;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;
    EXPECT_EQ(0u, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(pInHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenHighEndSkuWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturn256BitBandwidth) {
    auto testHwInfo = pInHwInfo;
    testHwInfo.gtSystemInfo.EUCount = 512;

    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;

    uint64_t expectedBandwidth = 800ULL * 1000 * 1000 * 256 / 8;
    EXPECT_EQ(expectedBandwidth, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenMidRangeSkuWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturn192BitBandwidth) {
    auto testHwInfo = pInHwInfo;
    testHwInfo.gtSystemInfo.EUCount = 358;

    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;

    uint64_t expectedBandwidth = 800ULL * 1000 * 1000 * 192 / 8;
    EXPECT_EQ(expectedBandwidth, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenEntryLevelSkuWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenReturn192BitBandwidth) {
    auto testHwInfo = pInHwInfo;
    testHwInfo.gtSystemInfo.EUCount = 256;

    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;

    uint64_t expectedBandwidth = 800ULL * 1000 * 1000 * 192 / 8;
    EXPECT_EQ(expectedBandwidth, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));
}

BMGTEST_F(BmgProductHelperLinux, givenBoundaryEuCountWhenGetDeviceMemoryMaxBandWidthInBytesPerSecondIsCalledThenCorrectBusWidthIsUsed) {
    auto testHwInfo = pInHwInfo;

    // Test exactly 448 EUs (boundary) - should use 256-bit
    testHwInfo.gtSystemInfo.EUCount = 448;
    drm->storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    drm->useBaseGetDeviceMemoryMaxClockRateInMhz = false;
    uint64_t expectedBandwidth = 800ULL * 1000 * 1000 * 256 / 8;
    EXPECT_EQ(expectedBandwidth, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));

    // Test 447 EUs (just below) - should use 192-bit
    testHwInfo.gtSystemInfo.EUCount = 447;
    expectedBandwidth = 800ULL * 1000 * 1000 * 192 / 8;
    EXPECT_EQ(expectedBandwidth, productHelper->getDeviceMemoryMaxBandWidthInBytesPerSecond(testHwInfo, osInterface, 0));
}
