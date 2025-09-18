/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern GT_SYSTEM_INFO passedGtSystemInfo;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

TEST(OsInterfaceTest, GivenLinuxWhenCallingAre64kbPagesEnabledThenReturnFalse) {
    EXPECT_FALSE(OSInterface::are64kbPagesEnabled());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenDeviceHandleQueriedThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    OSInterface osInterface;
    osInterface.setDriverModel(std::move(drm));
    EXPECT_EQ(0u, osInterface.getDriverModel()->getDeviceHandle());
}

TEST(OsInterfaceTest, GivenLinuxOsWhenCheckForNewResourceImplicitFlushSupportThenReturnTrue) {
    EXPECT_TRUE(OSInterface::newResourceImplicitFlush);
}

TEST(OsInterfaceTest, GivenLinuxOsWhenCheckForGpuIdleImplicitFlushSupportThenReturnFalse) {
    EXPECT_TRUE(OSInterface::gpuIdleImplicitFlush);
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenCallingIsDebugAttachAvailableThenFalseIsReturned) {
    OSInterface osInterface;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    EXPECT_FALSE(osInterface.isDebugAttachAvailable());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenCallingGetAggregatedProcessCountThenCallRedirectedToDriverModel) {
    OSInterface osInterface;
    EXPECT_EQ(0u, osInterface.getAggregatedProcessCount());

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    drm->mockProcessCount = 5;
    EXPECT_EQ(5u, osInterface.getAggregatedProcessCount());
}

TEST(OsInterfaceTest, whenOsInterfaceSetupGmmInputArgsThenArgsAreSet) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto drm = std::make_unique<DrmMock>(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::move(drm));

    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(&hwInfo->gtSystemInfo, passedInputArgs.pGtSysInfo);
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenGetThresholdForStagingCalledThenReturnThresholdForIntegratedDevices) {
    OSInterface osInterface;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    auto alignedPtr = reinterpret_cast<const void *>(2 * MemoryConstants::megaByte);
    EXPECT_TRUE(osInterface.isSizeWithinThresholdForStaging(alignedPtr, 16 * MemoryConstants::megaByte));
    EXPECT_FALSE(osInterface.isSizeWithinThresholdForStaging(alignedPtr, 64 * MemoryConstants::megaByte));

    auto misalignedPtr = reinterpret_cast<const void *>(3 * MemoryConstants::megaByte);
    EXPECT_TRUE(osInterface.isSizeWithinThresholdForStaging(misalignedPtr, 64 * MemoryConstants::megaByte));
    EXPECT_FALSE(osInterface.isSizeWithinThresholdForStaging(misalignedPtr, 512 * MemoryConstants::megaByte));
}

} // namespace NEO
