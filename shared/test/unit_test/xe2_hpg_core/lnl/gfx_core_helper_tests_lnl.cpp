/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_lnl.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestsLnl = GfxCoreHelperTest;

LNLTEST_F(GfxCoreHelperTestsLnl, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::commandBuffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

LNLTEST_F(GfxCoreHelperTestsLnl, WhenAskingForDcFlushThenReturnTrue) {
    EXPECT_NE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, this->pDevice->getRootDeviceEnvironment()), this->pDevice->getRootDeviceEnvironment().getProductHelper().isDcFlushMitigated());
}

LNLTEST_F(GfxCoreHelperTestsLnl, givenGetDeviceTimestampWidthCalledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = this->pDevice->getGfxCoreHelper();
    EXPECT_EQ(64u, helper.getDeviceTimestampWidth());

    debugManager.flags.OverrideTimestampWidth.set(36);
    EXPECT_EQ(36u, helper.getDeviceTimestampWidth());
}