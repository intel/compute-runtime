/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/xe_hpc_core/xe_hpc_core_test_ocl_fixtures.h"

#include "CL/cl_ext.h"

namespace NEO {

using ClGfxCoreHelperTestsPvcXt = Test<ClGfxCoreHelperXeHpcCoreFixture>;

PVCTEST_F(ClGfxCoreHelperTestsPvcXt, givenSingleTileCsrOnPvcXtWhenAllocatingCsrSpecificAllocationsAndIsNotBaseDieA0ThenStoredInProperMemoryPool) {
    auto hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo); // not BD A0
    checkIfSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInProperMemoryPool(&hwInfo);
}

PVCTEST_F(ClGfxCoreHelperTestsPvcXt, givenMultiTileCsrOnPvcWhenAllocatingCsrSpecificAllocationsAndIsNotBaseDieA0ThenStoredInLocalMemoryPool) {
    auto hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo); // not BD A0
    checkIfMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInLocalMemoryPool(&hwInfo);
}

PVCTEST_F(ClGfxCoreHelperTestsPvcXt, givenRelease1261WhenAskingForDeviceFeaturesThenReturnDp4a) {
    auto rootEnv = pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();

    auto deviceHwInfo = rootEnv->getMutableHardwareInfo();
    auto &productHelper = getHelper<ProductHelper>();

    deviceHwInfo->platform.usDeviceID = pvcXtVgDeviceIds.front();
    deviceHwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_C, *deviceHwInfo);
    deviceHwInfo->ipVersion.architecture = 12;
    deviceHwInfo->ipVersion.release = 61;
    deviceHwInfo->ipVersion.revision = deviceHwInfo->platform.usRevId;

    rootEnv->releaseHelper.reset();
    rootEnv->initReleaseHelper();

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    EXPECT_EQ(static_cast<cl_device_feature_capabilities_intel>(CL_DEVICE_FEATURE_FLAG_DP4A_INTEL), clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(*rootEnv));

    auto &compilerHelper = getHelper<CompilerProductHelper>();
    std::string extensions = compilerHelper.getDeviceExtensions(*deviceHwInfo, rootEnv->releaseHelper.get());

    EXPECT_EQ(std::string::npos, extensions.find("cl_intel_subgroup_matrix_multiply_accumulate"));
    EXPECT_EQ(std::string::npos, extensions.find("cl_intel_subgroup_split_matrix_multiply_accumulate"));
}

} // namespace NEO
