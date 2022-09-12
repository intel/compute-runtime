/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/system_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsNotCreatedAndIoctlsAreCalledOnce) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    EXPECT_FALSE(drm.querySystemInfo());

    EXPECT_EQ(nullptr, drm.getSystemInfo());
    EXPECT_EQ(1u + drm.getBaseIoctlCalls(), drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedWhenQueryingSpecificAtrributesThenReturnZero) {
    std::vector<uint8_t> inputData{};
    SystemInfo systemInfo(inputData);

    EXPECT_EQ(0u, systemInfo.getMemoryType());
    EXPECT_EQ(0u, systemInfo.getMaxMemoryChannels());
    EXPECT_EQ(0u, systemInfo.getNumThreadsPerEu());
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
    EXPECT_EQ(0u, systemInfo.getL3BankSizeInKb());
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFalseThenSystemInfoIsNotCreatedAndDebugMessageIsNotPrinted) {
    struct DrmMockToQuerySystemInfo : public DrmMock {
        DrmMockToQuerySystemInfo(RootDeviceEnvironment &rootDeviceEnvironment)
            : DrmMock(rootDeviceEnvironment) {}
        bool querySystemInfo() override { return false; }
    };

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockToQuerySystemInfo drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    ::testing::internal::CaptureStdout();

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_TRUE(isEmpty(::testing::internal::GetCapturedStdout()));
}

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsCreatedAndReturnsNonZeros) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
    EXPECT_NE(0u, systemInfo->getL3BankSizeInKb());

    EXPECT_EQ(2u + drm.getBaseIoctlCalls(), drm.ioctlCallsCount);
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
    EXPECT_EQ(0x2Du, systemInfo.getL3BankSizeInKb());
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFailsThenSystemInfoIsNotCreatedAndDebugMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    ::testing::internal::CaptureStdout();

    drm.failQueryDeviceBlob = true;

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_TRUE(hasSubstr(::testing::internal::GetCapturedStdout(), "INFO: System Info query failed!\n"));
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoSucceedsThenSystemInfoIsCreatedAndUsedToSetHardwareInfoAttributes) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;

    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

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

TEST(DrmSystemInfoTest, givenZeroBankCountWhenCreatingSystemInfoThenUseDualSubslicesToCalculateL3Size) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.L3BankCount = 0;

    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(0u, gtSystemInfo.L3BankCount);

    uint64_t expectedL3Size = gtSystemInfo.DualSubSliceCount * drm.getSystemInfo()->getL3BankSizeInKb();
    uint64_t calculatedL3Size = gtSystemInfo.L3CacheSizeInKb;

    EXPECT_EQ(expectedL3Size, calculatedL3Size);
}

TEST(DrmSystemInfoTest, givenNonZeroBankCountWhenCreatingSystemInfoThenUseDualSubslicesToCalculateL3Size) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.L3BankCount = 5;

    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_NE(0u, gtSystemInfo.L3BankCount);

    uint64_t expectedL3Size = gtSystemInfo.L3BankCount * drm.getSystemInfo()->getL3BankSizeInKb();
    uint64_t calculatedL3Size = gtSystemInfo.L3CacheSizeInKb;

    EXPECT_EQ(expectedL3Size, calculatedL3Size);
}
