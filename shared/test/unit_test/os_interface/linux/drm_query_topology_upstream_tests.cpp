/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

TEST(DrmQueryTopologyTest, GivenDrmWhenQueryingTopologyInfoCorrectMaxValuesAreSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    DrmQueryTopologyData topologyData = {};
    drm.engineInfoQueried = true;
    drm.systemInfoQueried = true;
    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(drm.storedSVal, topologyData.sliceCount);
    EXPECT_EQ(drm.storedSSVal, topologyData.subSliceCount);
    EXPECT_EQ(drm.storedEUVal, topologyData.euCount);

    EXPECT_EQ(drm.storedSVal, topologyData.maxSlices);
    EXPECT_EQ(drm.storedSSVal / drm.storedSVal, topologyData.maxSubSlicesPerSlice);
    EXPECT_EQ(drm.storedEUVal / drm.storedSSVal, topologyData.maxEusPerSubSlice);
}

TEST(DrmQueryTopologyTest, givenDrmWhenGettingSliceMappingsThenCorrectMappingReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drmMock{*executionEnvironment->rootDeviceEnvironments[0]};

    DrmQueryTopologyData topologyData = {};
    drmMock.engineInfoQueried = true;
    drmMock.systemInfoQueried = true;
    EXPECT_TRUE(drmMock.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    auto device0SliceMapping = drmMock.getSliceMappings(0);
    auto device1SliceMapping = drmMock.getSliceMappings(1);

    ASSERT_EQ(static_cast<size_t>(topologyData.maxSlices), device0SliceMapping.size());
    EXPECT_EQ(0u, device1SliceMapping.size());

    for (int i = 0; i < topologyData.maxSlices; i++) {
        EXPECT_EQ(i, device0SliceMapping[i]);
    }
}
