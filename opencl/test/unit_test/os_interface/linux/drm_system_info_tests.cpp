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

    EXPECT_FALSE(drm.querySystemInfo());

    EXPECT_EQ(nullptr, drm.getSystemInfo());
    EXPECT_EQ(0u, drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedWhenQueryingSpecificAtrributesThenReturnZero) {
    SystemInfoImpl systemInfo(nullptr, 0);

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

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoTrueThenSystemInfoIsNotCreatedAndDebugMessageIsNotPrinted) {
    struct DrmMockToQuerySystemInfo : public DrmMock {
        DrmMockToQuerySystemInfo(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        bool querySystemInfo() override { return true; }
    };

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMockToQuerySystemInfo drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo, GTTYPE_UNDEFINED};

    ::testing::internal::CaptureStdout();

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_THAT(::testing::internal::GetCapturedStdout(), ::testing::IsEmpty());
}

TEST(DrmSystemInfoTest, givenSystemInfoWhenSetupHardwareInfoThenFinishedWithSuccess) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo, GTTYPE_UNDEFINED};

    drm.systemInfo.reset(new SystemInfoImpl(nullptr, 0));

    ::testing::internal::CaptureStdout();
    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_THAT(::testing::internal::GetCapturedStdout(), ::testing::IsEmpty());
}
