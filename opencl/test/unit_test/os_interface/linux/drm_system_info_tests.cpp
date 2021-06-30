/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/system_info_impl.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gmock/gmock.h"

using namespace NEO;

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsNotCreatedAndNoIoctlsAreCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    EXPECT_TRUE(drm.querySystemInfo());

    EXPECT_EQ(nullptr, drm.getSystemInfo());
    EXPECT_EQ(0u, drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedWhenQueryingSpecificAtrributesThenReturnZero) {
    SystemInfoImpl systemInfo(nullptr, 0);

    EXPECT_EQ(0u, systemInfo.getL3CacheSizeInKb());
    EXPECT_EQ(0u, systemInfo.getL3BankCount());
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

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFailsThenSystemInfoIsNotCreatedAndDebugMessageIsPrinted) {
    struct DrmMockToFailQuerySystemInfo : public DrmMock {
        DrmMockToFailQuerySystemInfo(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        bool querySystemInfo() override { return false; }
    };

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMockToFailQuerySystemInfo drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo, GTTYPE_UNDEFINED};

    ::testing::internal::CaptureStdout();

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_THAT(::testing::internal::GetCapturedStdout(), ::testing::HasSubstr("INFO: System Info query failed!\n"));
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenSystemInfoIsCreatedThenSetHardwareInfoAttributesWithZeros) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo, GTTYPE_UNDEFINED};

    drm.systemInfo.reset(new SystemInfoImpl(nullptr, 0));
    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ_VAL(0u, gtSystemInfo.L3CacheSizeInKb);
    EXPECT_EQ(0u, gtSystemInfo.L3BankCount);
    EXPECT_EQ(0u, gtSystemInfo.MaxFillRate);
    EXPECT_EQ(0u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(0u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(0u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(0u, gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_EQ(0u, gtSystemInfo.MaxSlicesSupported);
    EXPECT_EQ(0u, gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(0u, gtSystemInfo.MaxDualSubSlicesSupported);
}
