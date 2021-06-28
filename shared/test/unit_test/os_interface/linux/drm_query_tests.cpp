/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmQueryTest, GivenGtMaxFreqFileExistsWhenFrequencyIsQueriedThenValidValueIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    int expectedMaxFrequency = 1000;

    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto hwInfo = *defaultHwInfo;

    std::string gtMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt_max_freq_mhz";

    EXPECT_TRUE(fileExists(gtMaxFreqFile));
    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}

TEST(DrmQueryTest, WhenCallingIsDebugAttachAvailableThenReturnValueIsFalse) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.allowDebugAttachCallBase = true;

    EXPECT_FALSE(drm.isDebugAttachAvailable());
}

TEST(DrmQueryTest, GivenDrmWhenQueryingTopologyInfoCorrectMaxValuesAreSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    Drm::QueryTopologyData topologyData = {};

    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(drm.storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drm.storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drm.storedEUVal, topologyData.euCount);

    EXPECT_EQ(drm.storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(drm.storedSSVal / drm.storedSVal, topologyData.maxSubSliceCount);
    EXPECT_EQ(drm.storedEUVal / drm.storedSSVal, topologyData.maxEuCount);
}

TEST(DrmQueryTest, givenDrmWhenGettingSliceMappingsThenCorrectMappingReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drmMock{*executionEnvironment->rootDeviceEnvironments[0]};

    Drm::QueryTopologyData topologyData = {};

    EXPECT_TRUE(drmMock.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    auto device0SliceMapping = drmMock.getSliceMappings(0);
    auto device1SliceMapping = drmMock.getSliceMappings(1);

    ASSERT_EQ(static_cast<size_t>(topologyData.maxSliceCount), device0SliceMapping.size());
    EXPECT_EQ(0u, device1SliceMapping.size());

    for (int i = 0; i < topologyData.maxSliceCount; i++) {
        EXPECT_EQ(i, device0SliceMapping[i]);
    }
}

using HwConfigTopologyQuery = ::testing::Test;

HWTEST2_F(HwConfigTopologyQuery, WhenGettingTopologyFailsThenSetMaxValuesBasedOnSubsliceIoctlQuery, MatchAny) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    drm->setGtType(GTTYPE_GT1);

    auto osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::unique_ptr<Drm>(drm));

    drm->failRetTopology = true;

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;

    hwInfo.gtSystemInfo.MaxSlicesSupported = 0;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 0;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 6;

    auto hwConfig = HwInfoConfigHw<productFamily>::get();
    int ret = hwConfig->configureHwInfoDrm(&hwInfo, &outHwInfo, osInterface.get());
    EXPECT_NE(-1, ret);

    EXPECT_EQ(6u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, outHwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(hwInfo.gtSystemInfo.SliceCount, outHwInfo.gtSystemInfo.MaxSlicesSupported);

    EXPECT_EQ(static_cast<uint32_t>(drm->storedEUVal), outHwInfo.gtSystemInfo.EUCount);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal), outHwInfo.gtSystemInfo.SubSliceCount);
}

TEST(DrmQueryTest, givenIoctlWhenParseToStringThenProperStringIsReturned) {
    for (auto ioctlCodeString : ioctlCodeStringMap) {
        EXPECT_STREQ(IoctlHelper::getIoctlString(ioctlCodeString.first).c_str(), ioctlCodeString.second);
    }
}

TEST(DrmQueryTest, givenIoctlParamWhenParseToStringThenProperStringIsReturned) {
    for (auto ioctlParamCodeString : ioctlParamCodeStringMap) {
        EXPECT_STREQ(IoctlHelper::getIoctlParamString(ioctlParamCodeString.first).c_str(), ioctlParamCodeString.second);
    }
}
