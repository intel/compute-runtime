/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/xe_hpc_core/xe_hpc_core_test_ocl_fixtures.h"

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
} // namespace NEO
