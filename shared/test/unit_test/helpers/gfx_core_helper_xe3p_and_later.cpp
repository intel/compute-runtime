/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using GfxCoreHelperXe3pAndLaterTests = GfxCoreHelperTest;

HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenAllocDataWhenSetExtraAllocationDataThenSetLocalMemForProperTypes, IsAtLeastXe3pCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    for (int type = 0; type < static_cast<int>(AllocationType::count); type++) {
        AllocationProperties allocProperties(0, 1, static_cast<AllocationType>(type), {});
        AllocationData allocData{};
        allocData.flags.useSystemMemory = true;
        allocData.flags.requiresCpuAccess = false;

        gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, pDevice->getRootDeviceEnvironment());

        if (defaultHwInfo->featureTable.flags.ftrLocalMemory) {
            if (allocProperties.allocationType == AllocationType::commandBuffer ||
                allocProperties.allocationType == AllocationType::ringBuffer) {
                EXPECT_FALSE(allocData.flags.useSystemMemory);
                EXPECT_TRUE(allocData.flags.requiresCpuAccess);
            } else if (allocProperties.allocationType == AllocationType::semaphoreBuffer) {
                if (getHelper<ProductHelper>().isAcquireGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo())) {
                    EXPECT_FALSE(allocData.flags.useSystemMemory);
                } else {
                    EXPECT_TRUE(allocData.flags.useSystemMemory);
                }
                EXPECT_TRUE(allocData.flags.requiresCpuAccess);
            } else {
                EXPECT_FALSE(allocData.flags.useSystemMemory);
                EXPECT_FALSE(allocData.flags.requiresCpuAccess);
            }
        }
    }
}
