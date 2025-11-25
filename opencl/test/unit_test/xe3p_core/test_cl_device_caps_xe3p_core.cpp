/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/test/common/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;

using Xe3pCoreClDeviceCaps = Test<ClDeviceFixture>;

XE3P_CORETEST_F(Xe3pCoreClDeviceCaps, givenXe3pCoreWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_bfloat16_conversions")));
}

XE3P_CORETEST_F(Xe3pCoreClDeviceCaps, givenXe3pCoreWhenCheckingCapsThenDeviceDoesSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(caps.independentForwardProgress);
}

XE3P_CORETEST_F(Xe3pCoreClDeviceCaps, givenXe3pCoreWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
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

using QueueFamilyNameTestXe3pCore = QueueFamilyNameTest;

XE3P_CORETEST_F(QueueFamilyNameTestXe3pCore, givenCccsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::renderCompute, "cccs");
}

XE3P_CORETEST_F(QueueFamilyNameTestXe3pCore, givenLinkedBcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::linkedCopy, "linked bcs");
}

XE3P_CORETEST_F(Xe3pCoreClDeviceCaps, givenXe3pCoreWhenCheckingCapsThenFp16AtomicAddIsReported) {
    const auto &caps = pClDevice->getDeviceInfo();

    uint64_t expectedFp16Caps = (0u | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::addAtomicCaps);
    EXPECT_EQ(caps.halfFpAtomicCapabilities, expectedFp16Caps);
}
