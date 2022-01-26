/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/system_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"

#include "gmock/gmock.h"

using namespace NEO;

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsNotCreatedAndIoctlsAreCalledOnce) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    EXPECT_FALSE(drm.querySystemInfo());

    EXPECT_EQ(nullptr, drm.getSystemInfo());
    EXPECT_EQ(1u, drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedWhenQueryingSpecificAtrributesThenReturnZero) {
    std::vector<uint8_t> inputData{};
    SystemInfo systemInfo(inputData);

    EXPECT_EQ(0u, systemInfo.getL3CacheSizeInKb());
    EXPECT_EQ(0u, systemInfo.getL3BankCount());
    EXPECT_EQ(0u, systemInfo.getMemoryType());
    EXPECT_EQ(0u, systemInfo.getMaxMemoryChannels());
    EXPECT_EQ(0u, systemInfo.getNumThreadsPerEu());
    EXPECT_EQ(0u, systemInfo.getMaxFillRate());
    EXPECT_EQ(0u, systemInfo.getTotalVsThreads());
    EXPECT_EQ(0u, systemInfo.getTotalHsThreads());
    EXPECT_EQ(0u, systemInfo.getTotalDsThreads());
    EXPECT_EQ(0u, systemInfo.getTotalGsThreads());
    EXPECT_EQ(0u, systemInfo.getTotalPsThreads());
    EXPECT_EQ(0u, systemInfo.getMaxEuPerDualSubSlice());
    EXPECT_EQ(0u, systemInfo.getMaxSlicesSupported());
    EXPECT_EQ(0u, systemInfo.getMaxDualSubSlicesSupported());
    EXPECT_EQ(0u, systemInfo.getMaxRCS());
    EXPECT_EQ(0u, systemInfo.getMaxCCS());
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFalseThenSystemInfoIsNotCreatedAndDebugMessageIsNotPrinted) {
    struct DrmMockToQuerySystemInfo : public DrmMock {
        DrmMockToQuerySystemInfo(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        bool querySystemInfo() override { return false; }
    };

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMockToQuerySystemInfo drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    ::testing::internal::CaptureStdout();

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_THAT(::testing::internal::GetCapturedStdout(), ::testing::IsEmpty());
}

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsCreatedAndReturnsNonZeros) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    EXPECT_TRUE(drm.querySystemInfo());

    auto systemInfo = drm.getSystemInfo();
    EXPECT_NE(nullptr, systemInfo);

    EXPECT_NE(0u, systemInfo->getMaxMemoryChannels());
    EXPECT_NE(0u, systemInfo->getMemoryType());
    EXPECT_NE(0u, systemInfo->getTotalVsThreads());
    EXPECT_NE(0u, systemInfo->getTotalHsThreads());
    EXPECT_NE(0u, systemInfo->getTotalDsThreads());
    EXPECT_NE(0u, systemInfo->getTotalGsThreads());
    EXPECT_NE(0u, systemInfo->getTotalPsThreads());
    EXPECT_NE(0u, systemInfo->getMaxEuPerDualSubSlice());
    EXPECT_NE(0u, systemInfo->getMaxSlicesSupported());
    EXPECT_NE(0u, systemInfo->getMaxDualSubSlicesSupported());
    EXPECT_NE(0u, systemInfo->getMaxDualSubSlicesSupported());
    EXPECT_NE(0u, systemInfo->getMaxRCS());
    EXPECT_NE(0u, systemInfo->getMaxCCS());

    EXPECT_EQ(2u, drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedFromDeviceBlobWhenQueryingSpecificAtrributesThenReturnCorrectValues) {
    SystemInfo systemInfo(inputBlobData);

    EXPECT_EQ(0x0Au, systemInfo.getMaxMemoryChannels());
    EXPECT_EQ(0x0Bu, systemInfo.getMemoryType());
    EXPECT_EQ(0x10u, systemInfo.getTotalVsThreads());
    EXPECT_EQ(0x12u, systemInfo.getTotalHsThreads());
    EXPECT_EQ(0x13u, systemInfo.getTotalDsThreads());
    EXPECT_EQ(0x11u, systemInfo.getTotalGsThreads());
    EXPECT_EQ(0x15u, systemInfo.getTotalPsThreads());
    EXPECT_EQ(0x03u, systemInfo.getMaxEuPerDualSubSlice());
    EXPECT_EQ(0x01u, systemInfo.getMaxSlicesSupported());
    EXPECT_EQ(0x02u, systemInfo.getMaxDualSubSlicesSupported());
    EXPECT_EQ(0x02u, systemInfo.getMaxDualSubSlicesSupported());
    EXPECT_EQ(0x17u, systemInfo.getMaxRCS());
    EXPECT_EQ(0x18u, systemInfo.getMaxCCS());
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFailsThenSystemInfoIsNotCreatedAndDebugMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    ::testing::internal::CaptureStdout();

    drm.failQueryDeviceBlob = true;

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_THAT(::testing::internal::GetCapturedStdout(), ::testing::HasSubstr("INFO: System Info query failed!\n"));
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoSucceedsThenSystemInfoIsCreatedAndUsedToSetHardwareInfoAttributes) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;

    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());

    EXPECT_GT(gtSystemInfo.TotalVsThreads, 0u);
    EXPECT_GT(gtSystemInfo.TotalHsThreads, 0u);
    EXPECT_GT(gtSystemInfo.TotalDsThreads, 0u);
    EXPECT_GT(gtSystemInfo.TotalGsThreads, 0u);
    EXPECT_GT(gtSystemInfo.TotalPsThreadsWindowerRange, 0u);
    EXPECT_GT(gtSystemInfo.TotalDsThreads, 0u);
    EXPECT_GT(gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_GT(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxSubSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MemoryType, 0u);
}
