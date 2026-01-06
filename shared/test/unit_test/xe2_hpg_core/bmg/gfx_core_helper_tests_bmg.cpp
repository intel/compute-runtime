/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_bmg.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

struct GfxCoreHelperTestsBmg : public GfxCoreHelperTest {
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        auto releaseHelper = ReleaseHelper::create(hardwareInfo.ipVersion);
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0, releaseHelper.get());
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {}
};

BMGTEST_F(GfxCoreHelperTestsBmg, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    setUpImpl();
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::commandBuffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager{*pDevice->getExecutionEnvironment()};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

BMGTEST_F(GfxCoreHelperTestsBmg, WhenAskingForDcFlushThenReturnFalse) {
    setUpImpl();
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()));
}
