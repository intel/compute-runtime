/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"

using namespace NEO;

using CompilerProductHelperPvcTest = ::testing::Test;

PVCTEST_F(CompilerProductHelperPvcTest, givenPvcConfigsWhenMatchConfigWithRevIdThenProperConfigIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();

    for (const auto &config : AOT_PVC::productConfigs) {
        if (config == AOT::PVC_XT_C0_VG) {
            continue;
        }

        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x0), AOT::PVC_XL_A0);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x1), AOT::PVC_XL_A0P);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x3), AOT::PVC_XT_A0);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x26), AOT::PVC_XT_B1);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x2f), AOT::PVC_XT_C0);
    }

    EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(AOT::PVC_XT_C0_VG, 0x2f), AOT::PVC_XT_C0_VG);
}

PVCTEST_F(CompilerProductHelperPvcTest, givenPvcWhenFailBuildProgramWithStatefulAccessPreferenceThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    EXPECT_FALSE(compilerProductHelper.failBuildProgramWithStatefulAccessPreference());
}

PVCTEST_F(CompilerProductHelperPvcTest, givenPvcB0AndLaterThenMatrixMultiplyAccumulateTF32IsSupported) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    auto hwInfo = *rootDeviceEnvironment.getHardwareInfo();

    for (auto revision : {REVISION_A0, REVISION_B}) {
        for (auto deviceId : pvcXlDeviceIds) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateTF32Supported(hwInfo));
        }
    }

    for (auto deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateTF32Supported(hwInfo));
    }

    for (auto revision : {REVISION_B, REVISION_C}) {
        for (auto deviceId : pvcXtDeviceIds) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
            EXPECT_TRUE(compilerProductHelper.isMatrixMultiplyAccumulateTF32Supported(hwInfo));
        }
    }

    for (auto deviceId : pvcXtVgDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_C, hwInfo);
        EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateTF32Supported(hwInfo));
    }
}
