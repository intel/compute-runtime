/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using DeviceTestsDg2 = ::testing::Test;

HWTEST_EXCLUDE_PRODUCT(DeviceTests, givenZexNumberOfCssEnvVariableSetAmbigouslyWhenDeviceIsCreatedThenDontApplyAnyLimitations, IGFX_DG2)

DG2TEST_F(DeviceTestsDg2, WhenAdjustCCSCountImplCalledThenHwInfoReportOneComputeEngine) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    auto hwInfo = *defaultHwInfo;

    hwInfo.featureTable.flags.ftrCCSNode = 1;
    hwInfo.gtSystemInfo.CCSInfo.IsValid = 1;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(&hwInfo);
    executionEnvironment->incRefInternal();
    executionEnvironment->adjustCcsCountImpl(executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(1u, executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
}
