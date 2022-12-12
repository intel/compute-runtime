/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/xe_hpc_core/xe_hpc_core_test_ocl_fixtures.h"

namespace NEO {

using ClGfxCoreHelperTestsPvcXt = Test<ClGfxCoreHelperXeHpcCoreFixture>;

PVCTEST_F(ClGfxCoreHelperTestsPvcXt, givenSingleTileCsrOnPvcXtWhenAllocatingCsrSpecificAllocationsAndIsNotBaseDieA0ThenStoredInProperMemoryPool) {
    auto hwInfo = *defaultHwInfo;
    const auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo); // not BD A0
    checkIfSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInProperMemoryPool(&hwInfo);
}

PVCTEST_F(ClGfxCoreHelperTestsPvcXt, givenMultiTileCsrOnPvcWhenAllocatingCsrSpecificAllocationsAndIsNotBaseDieA0ThenStoredInLocalMemoryPool) {
    auto hwInfo = *defaultHwInfo;
    const auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo); // not BD A0
    checkIfMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInLocalMemoryPool(&hwInfo);
}
} // namespace NEO
