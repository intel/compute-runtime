/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using DeviceTestsPvc = ::testing::Test;

HWTEST_EXCLUDE_PRODUCT(DeviceTests, givenZexNumberOfCssEnvVariableSetAmbigouslyWhenDeviceIsCreatedThenDontApplyAnyLimitations, IGFX_PVC)

PVCTEST_F(DeviceTestsPvc, WhenDeviceIsCreatedThenOnlyOneCcsEngineIsExposed) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(1);
    auto hwInfo = *defaultHwInfo;

    hwInfo.featureTable.flags.ftrCCSNode = 1;
    hwInfo.gtSystemInfo.CCSInfo.IsValid = 1;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto device = deviceFactory.rootDevices[0];

    auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::Compute);
    auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
    EXPECT_EQ(1u, computeEngineGroup.engines.size());
}
