/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

TEST(DrmQueryTopologyTest, GivenDrmWhenQueryingTopologyInfoCorrectMaxValuesAreSet) {
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

TEST(DrmQueryTopologyTest, givenDrmWhenGettingSliceMappingsThenCorrectMappingReturned) {
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
