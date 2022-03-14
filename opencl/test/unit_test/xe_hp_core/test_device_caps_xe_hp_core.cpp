/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

typedef Test<ClDeviceFixture> XE_HP_COREDeviceCaps;

HWCMDTEST_F(IGFX_XE_HP_CORE, XE_HP_COREDeviceCaps, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XE_HP_COREDeviceCaps, givenKernelThatDoesStatelessWritesWhenItIsCreatedThenItHasProperFieldSet) {
    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.mockKernel->initialize();
    bool statelessWritesEmitted = mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesStatelessWrites;
    EXPECT_EQ(statelessWritesEmitted, mockKernel.mockKernel->areStatelessWritesUsed());
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenXE_HP_COREWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenXE_HP_COREWhenCheckingImageSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenXE_HP_COREWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenXE_HP_COREWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenXE_HP_COREWhenDeviceCapsInitializedThenAddXE_HP_COREExtensions) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_dot_accumulate")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenXE_HP_COREWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(caps.independentForwardProgress);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenEnabledFtrPooledEuAndA0SteppingWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;
    PLATFORM &myPlatform = myHwInfo.platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(myPlatform.eProductFamily);

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;
    myPlatform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, myHwInfo);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 512u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenDeviceThatHasHighNumberOfExecutionUnitsAndA0SteppingWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo512) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(myPlatform.eProductFamily);

    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * 8;
    myPlatform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, myHwInfo);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(512u, device->sharedDeviceInfo.maxWorkGroupSize);
    EXPECT_EQ(device->sharedDeviceInfo.maxWorkGroupSize / 8, device->getDeviceInfo().maxNumOfSubGroups);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenEnabledFtrPooledEuAndNotA0SteppingWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;
    PLATFORM &myPlatform = myHwInfo.platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(myPlatform.eProductFamily);

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;
    myPlatform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, myHwInfo);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 1024u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenDeviceThatHasHighNumberOfExecutionUnitsAndNotA0SteppingWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo1024) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(myPlatform.eProductFamily);

    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * 8; // 128 threads per subslice, in simd 8 gives 1024
    myPlatform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, myHwInfo);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(1024u, device->sharedDeviceInfo.maxWorkGroupSize);
    EXPECT_EQ(device->sharedDeviceInfo.maxWorkGroupSize / 8, device->getDeviceInfo().maxNumOfSubGroups);
}

XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchAndMaxSubSlicesSupportedIsSmallerThanMinMaxSubSlicesSupportedThenReturnValidValue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    testSysInfo.MaxSubSlicesSupported = 24;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    uint32_t minMaxSubSlicesSupported = 32;
    uint32_t minCalculation = minMaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice *
                              hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;

    EXPECT_LE(testSysInfo.MaxSubSlicesSupported, minMaxSubSlicesSupported);
    EXPECT_EQ(minCalculation, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
}
XE_HP_CORE_TEST_F(XE_HP_COREDeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchAndMaxSubSlicesSupportedIsGreaterThanMinMaxSubSlicesSupportedThenReturnValidValue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &testSysInfo = hwInfo.gtSystemInfo;
    testSysInfo.MaxSubSlicesSupported = 40;

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    uint32_t minMaxSubSlicesSupported = 32;
    uint32_t minCalculation = minMaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice *
                              hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    uint32_t properCalculation = testSysInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice *
                                 hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;

    EXPECT_GT(testSysInfo.MaxSubSlicesSupported, minMaxSubSlicesSupported);
    EXPECT_GT(properCalculation, minCalculation);
    EXPECT_EQ(properCalculation, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
}

HWTEST_EXCLUDE_PRODUCT(DeviceGetCapsTest, givenEnabledFtrPooledEuWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(DeviceGetCapsTest, givenDeviceThatHasHighNumberOfExecutionUnitsWhenMaxWorkgroupSizeIsComputedItIsLimitedTo1024, IGFX_XE_HP_CORE);
