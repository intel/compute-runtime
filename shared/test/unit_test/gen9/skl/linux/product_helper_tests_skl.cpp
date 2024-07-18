/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "hw_cmds.h"

using namespace NEO;

struct SklProductHelperLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();
    }
};

SKLTEST_F(SklProductHelperLinux, WhenConfiguringHwInfoThenInformationIsCorrect) {
    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    // constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VDBoxInfo.Instances.Bits.VDBox0Enabled);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VDBoxInfo.IsValid);

    pInHwInfo.platform.usDeviceID = 0x1902;

    drm->storedSSVal = 3;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    pInHwInfo.platform.usDeviceID = 0x1917;

    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    pInHwInfo.platform.usDeviceID = 0x0903;

    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    pInHwInfo.platform.usDeviceID = 0x0903;

    drm->storedSSVal = 6;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(2u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    auto &outKmdNotifyProperties = outHwInfo.capabilityTable.kmdNotifyProperties;
    EXPECT_TRUE(outKmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, outKmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, outKmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, outKmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    EXPECT_FALSE(outKmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(0, outKmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

SKLTEST_F(SklProductHelperLinux, GivenFailedIoctlEuCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForEUVal = -4;
    drm->failRetTopology = true;

    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-4, ret);
}

SKLTEST_F(SklProductHelperLinux, GivenFailedIoctlSsCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForSSVal = -5;
    drm->failRetTopology = true;

    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(-5, ret);
}

SKLTEST_F(SklProductHelperLinux, GivenWaFlagsWhenConfiguringHwInfoThenInformationIsCorrect) {

    pInHwInfo.platform.usRevId = 1;
    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);

    pInHwInfo.platform.usRevId = 0;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCompressedResourceRequiresConstVA21);

    pInHwInfo.platform.usRevId = 5;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCompressedResourceRequiresConstVA21);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waDisablePerCtxtPreemptionGranularityControl);

    pInHwInfo.platform.usRevId = 6;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCompressedResourceRequiresConstVA21);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waDisablePerCtxtPreemptionGranularityControl);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCSRUncachable);
}

SKLTEST_F(SklProductHelperLinux, WhenConfiguringHwInfoThenEdramInformationIsCorrect) {

    int ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL(0u, outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x1926;

    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x1927;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x192D;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x193B;

    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((128u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    pInHwInfo.platform.usDeviceID = 0x193D;
    ret = productHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, getRootDeviceEnvironment());
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((128u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

template <typename T>
class SklHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<SklHw1x2x6, SklHw1x3x6, SklHw1x3x8, SklHw2x3x8, SklHw3x3x8> sklTestTypes;
TYPED_TEST_SUITE(SklHwInfoTests, sklTestTypes);
TYPED_TEST(SklHwInfoTests, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    DeviceDescriptor device = {0, &TypeParam::hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(ret, 0);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

TYPED_TEST(SklHwInfoTests, givenGTSystemInfoTypeWhenConfigureHardwareCustomThenSliceCountDontChange) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    TypeParam::setupHardwareInfo(&hwInfo, false, nullptr);
    auto sliceCount = gtSystemInfo.SliceCount;

    auto productHelper = ProductHelper::create(PRODUCT_FAMILY::IGFX_SKYLAKE);

    productHelper->configureHardwareCustom(&hwInfo, osInterface.get());
    EXPECT_EQ(gtSystemInfo.SliceCount, sliceCount);
}
