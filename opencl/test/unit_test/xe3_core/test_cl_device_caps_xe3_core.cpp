/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using Xe3CoreClDeviceCaps = Test<ClDeviceFixture>;

XE3_CORETEST_F(Xe3CoreClDeviceCaps, givenXe3CoreWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_bfloat16_conversions")));
}

XE3_CORETEST_F(Xe3CoreClDeviceCaps, givenXe3CoreWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(caps.independentForwardProgress);
}

XE3_CORETEST_F(Xe3CoreClDeviceCaps, givenXe3CoreWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 2048u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

XE3_CORETEST_F(Xe3CoreClDeviceCaps, givenDeviceExtensionsWhenDeviceCapsInitializedThenAddProperExtensions) {
    const auto &dInfo = pClDevice->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_khr_subgroup_named_barrier")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_extended_block_read")));
}

using QueueFamilyNameTestXe3Core = QueueFamilyNameTest;

XE3_CORETEST_F(QueueFamilyNameTestXe3Core, givenCccsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::renderCompute, "cccs");
}

XE3_CORETEST_F(QueueFamilyNameTestXe3Core, givenLinkedBcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::linkedCopy, "linked bcs");
}
