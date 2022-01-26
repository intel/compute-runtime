/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinuxSkl : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();
        drm->storedDeviceID = 0x0902;
    }
};

SKLTEST_F(HwInfoConfigTestLinuxSkl, WhenConfiguringHwInfoThenInformationIsCorrect) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.VDBoxInfo.Instances.Bits.VDBox0Enabled);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VEBoxInfo.IsValid);
    EXPECT_TRUE(outHwInfo.gtSystemInfo.VDBoxInfo.IsValid);

    drm->storedDeviceID = 0x1902;

    drm->storedSSVal = 3;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    drm->storedDeviceID = 0x1917;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    drm->storedDeviceID = 0x0903;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(aub_stream::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    drm->storedDeviceID = 0x0904;

    drm->storedSSVal = 6;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->storedDeviceID, outHwInfo.platform.usDeviceID);
    EXPECT_EQ((unsigned short)drm->storedDeviceRevID, outHwInfo.platform.usRevId);
    EXPECT_EQ((uint32_t)drm->storedEUVal, outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ((uint32_t)drm->storedSSVal, outHwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.gtSystemInfo.SliceCount);
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

SKLTEST_F(HwInfoConfigTestLinuxSkl, GivenUnknownDevIdWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedDeviceID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

SKLTEST_F(HwInfoConfigTestLinuxSkl, GivenFailedIoctlDevIdWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForDeviceID = -2;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

SKLTEST_F(HwInfoConfigTestLinuxSkl, GivenFailedIoctlDevRevIdWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForDeviceRevID = -3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

SKLTEST_F(HwInfoConfigTestLinuxSkl, GivenFailedIoctlEuCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForEUVal = -4;
    drm->failRetTopology = true;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

SKLTEST_F(HwInfoConfigTestLinuxSkl, GivenFailedIoctlSsCountWhenConfiguringHwInfoThenErrorIsReturned) {
    drm->storedRetValForSSVal = -5;
    drm->failRetTopology = true;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

SKLTEST_F(HwInfoConfigTestLinuxSkl, GivenWaFlagsWhenConfiguringHwInfoThenInformationIsCorrect) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->storedDeviceRevID = 1;
    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    drm->storedDeviceRevID = 0;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCompressedResourceRequiresConstVA21);

    drm->storedDeviceRevID = 5;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCompressedResourceRequiresConstVA21);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waDisablePerCtxtPreemptionGranularityControl);

    drm->storedDeviceRevID = 6;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCompressedResourceRequiresConstVA21);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waDisablePerCtxtPreemptionGranularityControl);
    EXPECT_EQ(0u, outHwInfo.workaroundTable.flags.waCSRUncachable);
}

SKLTEST_F(HwInfoConfigTestLinuxSkl, WhenConfiguringHwInfoThenEdramInformationIsCorrect) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    int ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL(0u, outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(0u, outHwInfo.featureTable.flags.ftrEDram);

    drm->storedDeviceID = 0x1926;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    drm->storedDeviceID = 0x1927;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    drm->storedDeviceID = 0x192D;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    drm->storedDeviceID = 0x193B;

    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((128u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);

    drm->storedDeviceID = 0x193D;
    ret = hwInfoConfig->configureHwInfoDrm(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((128u * 1024u), outHwInfo.gtSystemInfo.EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

template <typename T>
class SklHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<SKL_1x2x6, SKL_1x3x6, SKL_1x3x8, SKL_2x3x8, SKL_3x3x8> sklTestTypes;
TYPED_TEST_CASE(SklHwInfoTests, sklTestTypes);
TYPED_TEST(SklHwInfoTests, WhenGtIsSetupThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    DeviceDescriptor device = {0, &hwInfo, &TypeParam::setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);

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

    TypeParam::setupHardwareInfo(&hwInfo, false);
    auto sliceCount = gtSystemInfo.SliceCount;

    HwInfoConfig *hwConfig = HwInfoConfig::get(PRODUCT_FAMILY::IGFX_SKYLAKE);

    hwConfig->configureHardwareCustom(&hwInfo, osInterface.get());
    EXPECT_EQ(gtSystemInfo.SliceCount, sliceCount);
}
