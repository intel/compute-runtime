/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern GT_SYSTEM_INFO passedGtSystemInfo;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

} // namespace NEO

TEST(GmmClientContextTest, whenInitializeIsCalledThenClientContextHandleIsSet) {
    auto hwInfo = defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment{hwInfo};

    GmmClientContext clientContext{};
    clientContext.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, clientContext.getHandle());
}

TEST(GmmClientContextTest, givenNoOsInterfaceWhenIntializeIsCalledThenProperFlagsArePassedToGmmlib) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    GmmClientContext clientContext{};
    clientContext.initialize(rootDeviceEnvironment);

    EXPECT_EQ(nullptr, rootDeviceEnvironment.osInterface.get());
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(0, memcmp(&hwInfo->gtSystemInfo, &passedGtSystemInfo, sizeof(GT_SYSTEM_INFO)));
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}

TEST(GmmClientContextTest, givenOsInterfaceWhenIntializeIsCalledThenSetGmmInputArgsFuncIsCalled) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    auto &osInterface = *rootDeviceEnvironment.osInterface;
    osInterface.setDriverModel(std::make_unique<MockDriverModel>());
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    GmmClientContext clientContext{};
    clientContext.initialize(rootDeviceEnvironment);

    EXPECT_NE(nullptr, rootDeviceEnvironment.osInterface.get());
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(0, memcmp(&hwInfo->gtSystemInfo, &passedGtSystemInfo, sizeof(GT_SYSTEM_INFO)));
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);

    EXPECT_EQ(1u, static_cast<MockDriverModel *>(osInterface.getDriverModel())->setGmmInputArgsCalled);
}

TEST(GmmClientContextTest, givenEnableFtrTile64OptimizationDebugKeyWhenInitializeIsCalledThenProperValueIsPassedToGmmlib) {
    auto hwInfo = defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment{hwInfo};
    DebugManagerStateRestore restorer;
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    GmmClientContext clientContext{};
    {
        clientContext.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(-1);
        clientContext.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(0);
        clientContext.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(1);
        clientContext.initialize(*executionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(1u, passedFtrTable.FtrTile64Optimization);
    }
}
